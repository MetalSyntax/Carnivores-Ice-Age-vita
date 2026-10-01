/*
 * Copyright (C) 2026 Carnivores Ice Age Vita port contributors
 *
 * This software may be modified and distributed under the terms
 * of the MIT license. See the LICENSE file for details.
 */

/**
 * @file  input.c
 * @brief Front touch + physical controls -> IceAgeGLSurface touches.
 *
 * The engine only understands touches: IceAgeGLSurface forwards
 * touchesBegan/Moved/Ended(x, y) in surface pixels, and GUIControls.cpp maps
 * each one to a HUD control (or to the background, which rotates the camera).
 * Touches carry no ID: GUI_GetTouchByLocation() re-identifies a touch by the
 * stored location nearest to the event, so every move below is split into
 * small steps and each touch is ended at the exact spot it was last seen.
 *
 * Physical controls are turned into synthetic touches placed on the real HUD
 * controls, read at runtime from the engine's own gui_controls[] table
 * (0x180-byte records, layout from GUI_AddControl / GUI_PointInControl):
 *   +0x00 group      +0x04 subgroup mask   +0x08 type
 *   +0x0C x          +0x10 y               +0x1C w      +0x20 h
 *   +0x24 align flags (2: right, 4: h-center, 8: v-center)
 *   +0x2C scale      +0x32/+0x33 enabled/visible
 * Coordinates are the engine's 480x320 logical space, origin bottom-left
 * (GUI_RecalcTouchLocation: lx = x * scaleX, ly = (real_height - y) * scaleY).
 * A synthetic touch is only started if the control is active in the current
 * GUI group, so buttons do nothing in menus instead of hitting random widgets.
 *
 * The right stick does NOT use touches: input_patch() replaces
 * GUI_GetBackgroundMovements() (the per-frame sum of background drags that
 * Game_ProcessPlayerControls() turns into camera rotation, x 0.1875 x the
 * in-game sensitivity slider) and adds the stick to it, in logical px/second.
 * A synthetic drag was capped by the touch re-anchoring and scaled with the
 * frame rate (8 px/frame -> slow at the ~18 FPS of the 3D scenes).
 */

#include "input.h"

#include "utils/logger.h"
#include "utils/settings.h"

#include <psp2/ctrl.h>
#include <psp2/touch.h>
#include <psp2/kernel/processmgr.h>

#include <falso_jni/FalsoJNI.h>
#include <kubridge.h>
#include <vitaGL.h>
#include <so_util/so_util.h>

#include <math.h>
#include <stdint.h>
#include <string.h>

extern so_module so_mod;
extern jobject activity_obj;  // main.c
extern jobject surface_obj;   // main.c

#define CTL_STRIDE      0x180
#define LOGICAL_W       480.0f
#define LOGICAL_H       320.0f
// GUI_GetControllerVector(): the virtual stick saturates at 40 logical px.
#define STICK_RADIUS    40.0f
// Max logical distance per touchesMoved so GUI_GetTouchByLocation() keeps
// matching the same touch.
#define MAX_STEP        12.0f
#define ANALOG_DEADZONE 30
// Right stick at full deflection with look_sensitivity 100, in logical px/s.
#define LOOK_SPEED      720.0f
#define REAL_SLOTS      8
#define TOUCH_SLOTS     16      // gui_touched_locations[16][2]

typedef void (*touch_fn)(JNIEnv *env, jobject thiz, jfloat x, jfloat y);

static touch_fn touchesBegan, touchesMoved, touchesEnded, touchesCancelled;
static jboolean (*nativeOnBackPressed)(JNIEnv *env, jobject thiz);

static float *p_scaleX, *p_scaleY;
static int *p_real_width, *p_real_height;
static uint8_t *gui_controls;
static int *gui_controls_count, *gui_active_group;
static uint32_t *gui_active_subgroups;

static int *ctl_move, *ctl_fire, *ctl_alt_fire, *ctl_weapon, *ctl_binoculars,
           *ctl_call, *ctl_map, *ctl_photo_shot, *ctl_photo_zoom_in, *ctl_photo_zoom_out;
static int *ctl_fb_hunt, *ctl_fb_stats, *ctl_fb_trophy_stat, *ctl_fb_trophy, *ctl_fb_login;

typedef float (*gui_get_slider_value_fn)(int id);
typedef void (*gui_set_slider_value_fn)(int id, float val);
static gui_get_slider_value_fn GUI_GetSliderValue;
static gui_set_slider_value_fn GUI_SetSliderValue;

static int menu_focused_ctl = -1;
static uint64_t nav_repeat_timer;
static uint32_t nav_last_direction;

typedef struct {
    int active;
    float x, y;         // surface pixels, last position sent to the engine
    uint64_t since;     // begin timestamp (us)
} vtouch;

typedef struct {
    uint32_t buttons;   // any of these physical buttons holds the control
    int **control;      // engine global holding the control index
    vtouch t;
} button_map;

static button_map buttons[] = {
    { SCE_CTRL_RTRIGGER | SCE_CTRL_CROSS, &ctl_fire,       {0} },
    { SCE_CTRL_LTRIGGER,                  &ctl_alt_fire,   {0} },
    { SCE_CTRL_SQUARE,                    &ctl_weapon,     {0} },
    { SCE_CTRL_TRIANGLE,                  &ctl_binoculars, {0} },
    { SCE_CTRL_UP | SCE_CTRL_CIRCLE,      &ctl_call,       {0} },
    { SCE_CTRL_SELECT | SCE_CTRL_DOWN,    &ctl_map,        {0} },
};
#define NUM_BUTTONS (sizeof(buttons) / sizeof(buttons[0]))

static vtouch move_touch;
static float move_cx, move_cy;          // surface-space origin of move_touch

// GUI_GetBackgroundMovements() replacement state.
static float *touched_locations, *touched_start_locations;
static int *touched_controls;           // -1 = background touch, -500 = free
static float look_dx, look_dy;          // this frame's stick delta, logical px
static uint64_t look_last_us;

static struct {
    int id;             // SceTouchReport.id, -1 when free
    float x, y;
} real[REAL_SLOTS];

static uint32_t old_buttons;
static int ready;

/* --- engine space helpers ---------------------------------------------- */

static float scale_x(void) { return (p_scaleX && *p_scaleX > 0.0f) ? *p_scaleX : 0.5f; }
static float scale_y(void) { return (p_scaleY && *p_scaleY > 0.0f) ? *p_scaleY : LOGICAL_H / 544.0f; }
static float surf_w(void)  { return p_real_width && *p_real_width > 0 ? (float) *p_real_width : 960.0f; }
static float surf_h(void)  { return p_real_height && *p_real_height > 0 ? (float) *p_real_height : 544.0f; }

static void logical_to_surface(float lx, float ly, float *sx, float *sy) {
    *sx = lx / scale_x();
    *sy = surf_h() - ly / scale_y();
}

static uint8_t *control(int idx) {
    if (!gui_controls || !gui_controls_count || idx < 0 || idx >= *gui_controls_count)
        return NULL;
    return gui_controls + idx * CTL_STRIDE;
}

static int control_usable(int idx) {
    uint8_t *c = control(idx);
    if (!c || !gui_active_group || !gui_active_subgroups)
        return 0;
    return *(int *) c == *gui_active_group &&
           (*(uint32_t *) (c + 0x04) & *gui_active_subgroups) != 0 &&
           c[0x32] && c[0x33];
}

// Logical-space rectangle of a control, mirroring GUI_PointInControl().
static void control_rect(int idx, float *x0, float *y0, float *x1, float *y1) {
    uint8_t *c = control(idx);
    float s = *(float *) (c + 0x2C);
    float w = *(float *) (c + 0x1C) * s;
    float h = *(float *) (c + 0x20) * s;
    float x = *(float *) (c + 0x0C);
    float y = *(float *) (c + 0x10);
    uint32_t flags = *(uint32_t *) (c + 0x24);

    if (flags & 2) x -= w;
    if (flags & 4) x -= w * 0.5f;
    if (flags & 8) y -= h * 0.5f;

    *x0 = x; *x1 = x + w;
    *y0 = y; *y1 = y + h;
}

static int control_center_surface(int idx, float *sx, float *sy) {
    if (!control_usable(idx))
        return 0;
    float x0, y0, x1, y1;
    control_rect(idx, &x0, &y0, &x1, &y1);
    logical_to_surface((x0 + x1) * 0.5f, (y0 + y1) * 0.5f, sx, sy);
    return 1;
}

/* --- touch primitives --------------------------------------------------- */

static float clampf(float v, float lo, float hi) { return v < lo ? lo : (v > hi ? hi : v); }

static void t_begin(vtouch *t, float x, float y) {
    x = clampf(x, 0.0f, surf_w() - 1.0f);
    y = clampf(y, 0.0f, surf_h() - 1.0f);
    touchesBegan(&jni, surface_obj, x, y);
    t->active = 1;
    t->x = x;
    t->y = y;
    t->since = sceKernelGetProcessTimeWide();
}

static void t_move(vtouch *t, float x, float y) {
    if (!t->active)
        return;
    x = clampf(x, 0.0f, surf_w() - 1.0f);
    y = clampf(y, 0.0f, surf_h() - 1.0f);

    // Split into steps of at most MAX_STEP logical px.
    float dx = (x - t->x) * scale_x();
    float dy = (y - t->y) * scale_y();
    float dist = sqrtf(dx * dx + dy * dy);
    int steps = (int) ceilf(dist / MAX_STEP);
    if (steps < 1) {
        if (x == t->x && y == t->y)
            return;
        steps = 1;
    }
    float sx = t->x, sy = t->y;
    for (int i = 1; i <= steps; i++) {
        float k = (float) i / (float) steps;
        touchesMoved(&jni, surface_obj, sx + (x - sx) * k, sy + (y - sy) * k);
    }
    t->x = x;
    t->y = y;
}

static void t_end(vtouch *t) {
    if (!t->active)
        return;
    touchesEnded(&jni, surface_obj, t->x, t->y);
    t->active = 0;
}

/* --- front touch panel -------------------------------------------------- */

static void update_real_touch(void) {
    SceTouchData touch;
    if (sceTouchPeek(SCE_TOUCH_PORT_FRONT, &touch, 1) < 0)
        touch.reportNum = 0;

    int seen[REAL_SLOTS] = {0};
    float sw = surf_w(), sh = surf_h();

    for (int r = 0; r < (int) touch.reportNum && r < SCE_TOUCH_MAX_REPORT; r++) {
        int id = touch.report[r].id;
        float x = clampf(touch.report[r].x * sw / 1920.0f, 0.0f, sw - 1.0f);
        float y = clampf(touch.report[r].y * sh / 1088.0f, 0.0f, sh - 1.0f);

        int slot = -1;
        for (int s = 0; s < REAL_SLOTS; s++) {
            if (real[s].id == id) { slot = s; break; }
        }

        if (slot < 0) {
            for (int s = 0; s < REAL_SLOTS; s++) {
                if (real[s].id == -1) { slot = s; break; }
            }
            if (slot < 0)
                continue; // more fingers than slots: ignore the extra one
            real[slot].id = id;
            real[slot].x = x;
            real[slot].y = y;
            touchesBegan(&jni, surface_obj, x, y);
        } else if (real[slot].x != x || real[slot].y != y) {
            real[slot].x = x;
            real[slot].y = y;
            touchesMoved(&jni, surface_obj, x, y);
        }
        seen[slot] = 1;
    }

    for (int s = 0; s < REAL_SLOTS; s++) {
        if (real[s].id != -1 && !seen[s]) {
            touchesEnded(&jni, surface_obj, real[s].x, real[s].y);
            real[s].id = -1;
        }
    }
}

/* --- physical controls -------------------------------------------------- */

static float axis(uint8_t v) {
    int d = (int) v - 128;
    if (d > -ANALOG_DEADZONE && d < ANALOG_DEADZONE)
        return 0.0f;
    float f = (d > 0 ? (float) (d - ANALOG_DEADZONE) : (float) (d + ANALOG_DEADZONE))
              / (127.0f - ANALOG_DEADZONE);
    return clampf(f, -1.0f, 1.0f);
}

static void update_move(float ax, float ay) {
    int idx = ctl_move ? *ctl_move : -1;

    if (ax == 0.0f && ay == 0.0f) {
        t_end(&move_touch);
        return;
    }

    if (!move_touch.active) {
        if (!control_center_surface(idx, &move_cx, &move_cy))
            return;
        t_begin(&move_touch, move_cx, move_cy);
    }

    // Stick up (ay < 0) = surface up = logical up = forward.
    float tx = move_cx + ax * STICK_RADIUS / scale_x();
    float ty = move_cy + ay * STICK_RADIUS / scale_y();
    t_move(&move_touch, tx, ty);
}

// Engine's GUI_GetBackgroundMovements(float *dx, float *dy), reimplemented
// 1:1 from the pseudo-C (sum of location - start over background touches,
// then start = location) plus the right stick delta of this frame. Only
// caller: Game_ProcessPlayerControls(), i.e. in-game camera only.
static void GUI_GetBackgroundMovements_hook(float *dx, float *dy) {
    float sx = 0.0f, sy = 0.0f;
    for (int i = 0; i < TOUCH_SLOTS; i++) {
        if (touched_controls[i] != -1)
            continue;
        float *loc = &touched_locations[i * 2], *start = &touched_start_locations[i * 2];
        sx += loc[0] - start[0];
        sy += loc[1] - start[1];
        start[0] = loc[0];
        start[1] = loc[1];
    }
    *dx = sx + look_dx;
    *dy = sy + look_dy;
    look_dx = look_dy = 0.0f;
}

static void update_look(float ax, float ay) {
    uint64_t now = sceKernelGetProcessTimeWide();
    float dt = look_last_us ? (float) (now - look_last_us) / 1000000.0f : 0.0f;
    look_last_us = now;
    if (dt > 0.1f)
        dt = 0.1f; // after a long frame (loading), don't jump

    // Only while the in-game HUD is up; the hook is not called elsewhere, and
    // a stale delta must not be applied when the game resumes.
    if ((ax == 0.0f && ay == 0.0f) || !control_usable(ctl_move ? *ctl_move : -1)) {
        look_dx = look_dy = 0.0f;
        return;
    }

    // Blended linear and quadratic response curve for responsive aiming. Logical space
    // is y-up, so stick up (ay < 0) is a positive y drag, like a finger
    // moving up the screen.
    float speed = LOOK_SPEED * (float) setting_lookSensitivity / 100.0f * dt;
    float norm_ax = ax;
    float norm_ay = ay;
    float curve_x = 0.4f * norm_ax + 0.6f * norm_ax * fabsf(norm_ax);
    float curve_y = 0.4f * norm_ay + 0.6f * norm_ay * fabsf(norm_ay);
    look_dx = curve_x * speed;
    look_dy = -curve_y * speed * (setting_invertLookY ? -1.0f : 1.0f);
}

static void update_buttons(uint32_t held, uint32_t pressed) {
    for (unsigned i = 0; i < NUM_BUTTONS; i++) {
        button_map *b = &buttons[i];
        int down = (held & b->buttons) != 0;

        if (down && !b->t.active && (pressed & b->buttons)) {
            float sx, sy;
            int idx = *b->control ? **b->control : -1;
            if (control_center_surface(idx, &sx, &sy))
                t_begin(&b->t, sx, sy);
        } else if (!down && b->t.active) {
            t_end(&b->t);
        }
    }
}

/* --- Menu navigation & Facebook suppression ----------------------------- *
 * Menus are driven with the D-pad / left stick + CROSS: the focused control
 * gets a synthetic tap at its center, exactly like a finger. The engine's
 * per-control touch state (+0x34 held, +0x35 release latch read by
 * GUI_ControlIsPressed) is only borrowed for the duration of GUI_DrawControls():
 * +0x34 = 1 makes the engine draw the focused button in its red "held" state,
 * and the original value is put back right after, so touch logic never sees
 * it. Hunt menu cells are drawn by Menu_Draw*Button() and have no "held"
 * sprite, so a frame is also painted after GUI_DrawControls().
 *
 * Gameplay vs. menu: in game the active group is 9 and the HUD subgroup
 * changes with the state (1 walking, 0x800 weapon drawn -- the movement stick
 * is NOT in 0x800 --, 0x4000 photo mode...). Gameplay = any HUD control the
 * physical buttons drive is usable; everything else (main menus, hunt setup,
 * in-game pause/statistics/relocate) is navigated as a menu. */

#define CTL_TYPE_BUTTON 0
#define CTL_TYPE_SLIDER 1       // GUI_SetSliderValue/Params: value +0x174, min +0x178, max +0x17C
#define CTL_TOUCH_HELD  0x34
#define CTL_TOUCH_LATCH 0x35
#define SLIDER_STEPS    20.0f

static int in_gameplay(void) {
    int *hud[] = { ctl_move, ctl_fire, ctl_weapon, ctl_photo_shot };
    for (unsigned i = 0; i < sizeof(hud) / sizeof(hud[0]); i++)
        if (hud[i] && control_usable(*hud[i]))
            return 1;
    return 0;
}

static int **fb_controls[] = { &ctl_fb_hunt, &ctl_fb_stats, &ctl_fb_trophy_stat, &ctl_fb_trophy,
                               &ctl_fb_login };
#define NUM_FB (sizeof(fb_controls) / sizeof(fb_controls[0]))

static int is_fb_control(int idx) {
    for (unsigned i = 0; i < NUM_FB; i++)
        if (*fb_controls[i] && **fb_controls[i] == idx)
            return 1;
    return 0;
}

// Facebook is gone on Vita. All controls are created once by Menu_Init()
// (game_movement_controller is control 0), so an index of 0 here only means
// "not created yet" and must not be touched. The engine re-enables the share
// buttons every frame on the statistics/trophy screens; hiding them right
// before input and drawing, and dropping any release latch, keeps them inert.
static void hide_social_controls(void) {
    for (unsigned i = 0; i < NUM_FB; i++) {
        int idx = *fb_controls[i] ? **fb_controls[i] : -1;
        uint8_t *c = idx > 0 ? control(idx) : NULL;
        if (!c)
            continue;
        c[0x32] = 0;
        c[0x33] = 0;
        c[CTL_TOUCH_LATCH] = 0;
    }
}

static int is_menu_control_navigable(int idx) {
    if (!control_usable(idx) || is_fb_control(idx))
        return 0;
    uint8_t *c = control(idx);
    int type = *(int *) (c + 0x08);
    if (type != CTL_TYPE_BUTTON && type != CTL_TYPE_SLIDER)
        return 0;
    float x0, y0, x1, y1;
    control_rect(idx, &x0, &y0, &x1, &y1);
    float w = x1 - x0, h = y1 - y0;
    // Skip hit areas that are tiny or cover the whole screen.
    if (w < 10.0f || h < 10.0f || (w > 450.0f && h > 280.0f))
        return 0;
    // Hunt pages slide horizontally: cells of the other pages stay usable
    // but sit off screen.
    float cx = (x0 + x1) * 0.5f, cy = (y0 + y1) * 0.5f;
    float lw = surf_w() * scale_x(), lh = surf_h() * scale_y();
    return cx >= 0.0f && cx <= lw && cy >= 0.0f && cy <= lh;
}

static int nav_center(int idx, float *sx, float *sy) {
    return is_menu_control_navigable(idx) && control_center_surface(idx, sx, sy);
}

static int find_initial_menu_focus(void) {
    int best = -1;
    float best_score = 1e9f;
    for (int i = 0; gui_controls_count && i < *gui_controls_count; i++) {
        float sx, sy;
        if (!nav_center(i, &sx, &sy))
            continue;
        float score = sy * 2.0f + sx;  // top-most, then left-most
        if (score < best_score) {
            best_score = score;
            best = i;
        }
    }
    return best;
}

// Nearest control in the pressed direction (surface space, y down); if there
// is none, wrap to the farthest one on the opposite side.
static int find_next_menu_control(int cur, uint32_t dir) {
    float cur_x, cur_y;
    if (!nav_center(cur, &cur_x, &cur_y))
        return find_initial_menu_focus();

    int best = -1, wrap = -1;
    float best_dist = 1e9f, wrap_dist = -1.0f;

    for (int i = 0; i < *gui_controls_count; i++) {
        float cx, cy;
        if (i == cur || !nav_center(i, &cx, &cy))
            continue;
        float dx = cx - cur_x, dy = cy - cur_y;
        float along, across;
        if (dir & SCE_CTRL_DOWN)       { along =  dy; across = dx; }
        else if (dir & SCE_CTRL_UP)    { along = -dy; across = dx; }
        else if (dir & SCE_CTRL_RIGHT) { along =  dx; across = dy; }
        else                           { along = -dx; across = dy; }

        if (along > 8.0f) {
            float dist = along + 2.0f * fabsf(across);
            if (dist < best_dist) { best_dist = dist; best = i; }
        } else if (along < -8.0f && fabsf(across) < 40.0f) {
            if (-along > wrap_dist) { wrap_dist = -along; wrap = i; }
        }
    }
    return best >= 0 ? best : (wrap >= 0 ? wrap : cur);
}

static void step_slider(int idx, int dir) {
    if (!GUI_GetSliderValue || !GUI_SetSliderValue)
        return;
    uint8_t *c = control(idx);
    float lo = *(float *) (c + 0x178), hi = *(float *) (c + 0x17C);
    if (!(hi > lo))
        return;
    float v = GUI_GetSliderValue(idx) + (float) dir * (hi - lo) / SLIDER_STEPS;
    GUI_SetSliderValue(idx, clampf(v, lo, hi));
}

static void update_menu_navigation(uint32_t held, uint32_t pressed, float ax, float ay) {
    if (!is_menu_control_navigable(menu_focused_ctl))
        menu_focused_ctl = find_initial_menu_focus();

    uint32_t dir = 0;
    if ((held & SCE_CTRL_UP)    || ay < -0.5f) dir = SCE_CTRL_UP;
    else if ((held & SCE_CTRL_DOWN)  || ay > 0.5f) dir = SCE_CTRL_DOWN;
    else if ((held & SCE_CTRL_LEFT)  || ax < -0.5f) dir = SCE_CTRL_LEFT;
    else if ((held & SCE_CTRL_RIGHT) || ax > 0.5f) dir = SCE_CTRL_RIGHT;

    uint64_t now = sceKernelGetProcessTimeWide();
    int step = 0;
    if (dir && dir != nav_last_direction) {
        step = 1;
        nav_repeat_timer = now + 280000;
    } else if (dir && now >= nav_repeat_timer) {
        step = 1;
        nav_repeat_timer = now + 120000;
    }
    nav_last_direction = dir;

    if (step && menu_focused_ctl >= 0) {
        uint8_t *c = control(menu_focused_ctl);
        if (*(int *) (c + 0x08) == CTL_TYPE_SLIDER && (dir & (SCE_CTRL_LEFT | SCE_CTRL_RIGHT)))
            step_slider(menu_focused_ctl, (dir & SCE_CTRL_LEFT) ? -1 : 1);
        else
            menu_focused_ctl = find_next_menu_control(menu_focused_ctl, dir);
    }

    // CROSS: tap the focused control.
    float sx, sy;
    if ((pressed & SCE_CTRL_CROSS) && nav_center(menu_focused_ctl, &sx, &sy)) {
        touchesBegan(&jni, surface_obj, sx, sy);
        touchesEnded(&jni, surface_obj, sx, sy);
    }

    // CIRCLE: back, like START / the Android back key.
    if ((pressed & SCE_CTRL_CIRCLE) && nativeOnBackPressed)
        nativeOnBackPressed(&jni, activity_obj);
}

// Focus box in logical space, same look as the Dinosaur Hunter port: a
// translucent yellow fill (brighter while CROSS is held) plus a solid 1.5 px
// yellow border, fitted to the control's own hit rectangle (GUI_PointInControl
// rect, so it follows each element's size, scale and alignment).
// GUI_DrawControls() only queues sprites (Sprites_Render() draws them at the
// end of Render()), so drawing there leaves the box under the menu art. It is
// drawn from a Font_Render() hook instead: Render() ends with Sprites_Render(),
// Font_Render(), GUI_RenderFade(), all in the 2D GUI projection whose units
// are the gui_controls[] logical space -- the same spot the Dinosaur Hunter
// port uses. GL state touched here is restored before returning.
#define FOCUS_MAX_QUADS 5

static GLfloat focus_v[FOCUS_MAX_QUADS * 12];
static uint32_t focus_c[FOCUS_MAX_QUADS * 6];
static int focus_quads;

static void focus_quad(float x0, float y0, float x1, float y1, uint32_t abgr) {
    if (focus_quads >= FOCUS_MAX_QUADS)
        return;
    GLfloat *v = &focus_v[focus_quads * 12];
    v[0] = x0; v[1]  = y0;  v[2] = x1;  v[3] = y0;  v[4]  = x1; v[5]  = y1;
    v[6] = x0; v[7]  = y0;  v[8] = x1;  v[9] = y1;  v[10] = x0; v[11] = y1;
    for (int i = 0; i < 6; i++)
        focus_c[focus_quads * 6 + i] = abgr;
    focus_quads++;
}

static void draw_menu_focus(void) {
    if (in_gameplay() || !is_menu_control_navigable(menu_focused_ctl))
        return;

    float x0, y0, x1, y1;
    const float t = 1.5f;
    control_rect(menu_focused_ctl, &x0, &y0, &x1, &y1);

    focus_quads = 0;
    focus_quad(x0, y0, x1, y1, (old_buttons & SCE_CTRL_CROSS) ? 0x5000c8ff : 0x2800c8ff);
    focus_quad(x0 - t, y0 - t, x1 + t, y0,     0xff00c8ff);
    focus_quad(x0 - t, y1,     x1 + t, y1 + t, 0xff00c8ff);
    focus_quad(x0 - t, y0,     x0,     y1,     0xff00c8ff);
    focus_quad(x1,     y0,     x1 + t, y1,     0xff00c8ff);

    GLboolean tex = glIsEnabled(GL_TEXTURE_2D);
    GLboolean blend = glIsEnabled(GL_BLEND);
    GLboolean col_arr = glIsEnabled(GL_COLOR_ARRAY);
    GLboolean tc_arr = glIsEnabled(GL_TEXTURE_COORD_ARRAY);
    GLboolean v_arr = glIsEnabled(GL_VERTEX_ARRAY);

    if (tex) glDisable(GL_TEXTURE_2D);
    if (!blend) glEnable(GL_BLEND);
    if (tc_arr) glDisableClientState(GL_TEXTURE_COORD_ARRAY);
    if (!v_arr) glEnableClientState(GL_VERTEX_ARRAY);
    if (!col_arr) glEnableClientState(GL_COLOR_ARRAY);

    glVertexPointer(2, GL_FLOAT, 0, focus_v);
    glColorPointer(4, GL_UNSIGNED_BYTE, 0, focus_c);
    glDrawArrays(GL_TRIANGLES, 0, focus_quads * 6);

    if (!col_arr) glDisableClientState(GL_COLOR_ARRAY);
    if (!v_arr) glDisableClientState(GL_VERTEX_ARRAY);
    if (tc_arr) glEnableClientState(GL_TEXTURE_COORD_ARRAY);
    if (!blend) glDisable(GL_BLEND);
    if (tex) glEnable(GL_TEXTURE_2D);
}

static so_hook font_render_hook;

static void Font_Render_hook(void) {
    SO_CONTINUE(int, font_render_hook);
    draw_menu_focus();
}

/* --- HUD opacity --------------------------------------------------------- *
 * The in-game touch buttons are redundant with the physical controls, so
 * GUI_DrawControls() is wrapped to draw them with hud_opacity % of their alpha.
 * The color is ARGB at +0x28 (GUI_SetControlColor; the engine uses 0x80ffffff
 * and animates game_fire's alpha every frame), so it is scaled only for the
 * duration of the draw and restored afterwards. The compass/minimap is drawn by
 * Navigations_Render(), not by gui_controls[], and keeps its opacity; so do
 * the weapon and call selection lists (game_weapons[], game_call_icons[]).
 * game_map and game_menu have no sprite (invisible hit areas). The same wrap
 * hides the Facebook buttons and gives the menu focus its red "held" look. */

#define CTL_COLOR 0x28

static so_hook draw_controls_hook;

static void GUI_DrawControls_hook(void) {
    hide_social_controls();

    int **hud[] = { &ctl_move, &ctl_fire, &ctl_alt_fire, &ctl_weapon, &ctl_binoculars,
                    &ctl_call, &ctl_photo_shot, &ctl_photo_zoom_in, &ctl_photo_zoom_out };
    uint32_t saved[sizeof(hud) / sizeof(hud[0])];
    uint32_t *color[sizeof(hud) / sizeof(hud[0])];
    int scale_alpha = setting_hudOpacity < 100;

    // Menu focus: red "held" look, only while drawing.
    uint8_t *focus = (!in_gameplay() && is_menu_control_navigable(menu_focused_ctl))
                     ? control(menu_focused_ctl) : NULL;
    uint8_t focus_held = focus ? focus[CTL_TOUCH_HELD] : 0;
    if (focus)
        focus[CTL_TOUCH_HELD] = 1;

    for (unsigned i = 0; i < sizeof(hud) / sizeof(hud[0]); i++) {
        uint8_t *c = (scale_alpha && *hud[i]) ? control(**hud[i]) : NULL;
        color[i] = c ? (uint32_t *) (c + CTL_COLOR) : NULL;
        if (!color[i])
            continue;
        saved[i] = *color[i];
        uint32_t a = (saved[i] >> 24) * (uint32_t) setting_hudOpacity / 100;
        if (a == 0 && setting_hudOpacity > 0 && (saved[i] >> 24))
            a = 1;
        *color[i] = (saved[i] & 0x00ffffff) | (a << 24);
    }

    kuKernelCpuUnrestrictedMemcpy((void *) draw_controls_hook.addr, draw_controls_hook.orig_instr,
                                  sizeof(draw_controls_hook.orig_instr));
    kuKernelFlushCaches((void *) draw_controls_hook.addr, sizeof(draw_controls_hook.orig_instr));
    ((void (*)(void)) draw_controls_hook.addr)();   // ARM
    kuKernelCpuUnrestrictedMemcpy((void *) draw_controls_hook.addr, draw_controls_hook.patch_instr,
                                  sizeof(draw_controls_hook.patch_instr));
    kuKernelFlushCaches((void *) draw_controls_hook.addr, sizeof(draw_controls_hook.patch_instr));

    for (unsigned i = 0; i < sizeof(hud) / sizeof(hud[0]); i++)
        if (color[i])
            *color[i] = saved[i];
    if (focus)
        focus[CTL_TOUCH_HELD] = focus_held;
}

/* --- public API --------------------------------------------------------- */

#define SYM(var, name) do { \
        var = (void *) so_symbol(&so_mod, name); \
        if (!var) l_error("input: symbol %s not found", name); \
    } while (0)

void input_patch(void) {
    uintptr_t draw = so_symbol(&so_mod, "_Z16GUI_DrawControlsv");
    if (draw && !(draw & 1))
        draw_controls_hook = hook_addr(draw, (uintptr_t) &GUI_DrawControls_hook);
    else
        l_warn("input: GUI_DrawControls not hooked");

    // Menu focus box, drawn after the engine's text (ARM, checked with objdump).
    uintptr_t fr = so_symbol(&so_mod, "_Z11Font_Renderv");
    if (fr && !(fr & 1))
        font_render_hook = hook_addr(fr, (uintptr_t) &Font_Render_hook);
    else
        l_warn("input: Font_Render not hooked, menu focus box disabled");

    touched_locations       = (float *) so_symbol(&so_mod, "gui_touched_locations");
    touched_start_locations = (float *) so_symbol(&so_mod, "gui_touched_start_locations");
    touched_controls        = (int *) so_symbol(&so_mod, "gui_touched_controls");
    uintptr_t fn = so_symbol(&so_mod, "_Z26GUI_GetBackgroundMovementsPfS_");
    if (!touched_locations || !touched_start_locations || !touched_controls || !fn) {
        l_error("input: GUI_GetBackgroundMovements not hooked, right stick camera disabled");
        return;
    }
    hook_addr(fn, (uintptr_t) &GUI_GetBackgroundMovements_hook);
}

void input_init(void) {
    SYM(touchesBegan,        "Java_com_tatem_iceage_IceAgeGLSurface_touchesBegan");
    SYM(touchesMoved,        "Java_com_tatem_iceage_IceAgeGLSurface_touchesMoved");
    SYM(touchesEnded,        "Java_com_tatem_iceage_IceAgeGLSurface_touchesEnded");
    SYM(touchesCancelled,    "Java_com_tatem_iceage_IceAgeGLSurface_touchesCancelled");
    SYM(nativeOnBackPressed, "Java_com_tatem_iceage_IceAgeAndroid_nativeOnBackPressed");

    SYM(p_scaleX,             "scaleX");
    SYM(p_scaleY,             "scaleY");
    SYM(p_real_width,         "real_width");
    SYM(p_real_height,        "real_height");
    SYM(gui_controls,         "gui_controls");
    SYM(gui_controls_count,   "gui_controls_count");
    SYM(gui_active_group,     "gui_active_group");
    SYM(gui_active_subgroups, "gui_active_subgroups");

    SYM(ctl_move,       "game_movement_controller");
    SYM(ctl_fire,       "game_fire");
    SYM(ctl_alt_fire,   "game_alternative_fire");
    SYM(ctl_weapon,     "game_weapon");
    SYM(ctl_binoculars, "game_binoculars");
    SYM(ctl_call,       "game_call");
    SYM(ctl_map,        "game_map");
    SYM(ctl_photo_shot,     "game_photomode_shot");
    SYM(ctl_photo_zoom_in,  "game_photomode_zoom_in");
    SYM(ctl_photo_zoom_out, "game_photomode_zoom_out");

    ctl_fb_hunt        = (int *) so_symbol(&so_mod, "game_share_hunt_statistic_with_facebook");
    ctl_fb_stats       = (int *) so_symbol(&so_mod, "game_share_statistics_with_facebook");
    ctl_fb_trophy_stat = (int *) so_symbol(&so_mod, "game_share_trophy_statistic_with_facebook");
    ctl_fb_trophy      = (int *) so_symbol(&so_mod, "game_share_trophy_with_facebook");
    ctl_fb_login       = (int *) so_symbol(&so_mod, "menu_options_facebook_login");

    GUI_GetSliderValue = (gui_get_slider_value_fn) so_symbol(&so_mod, "_Z18GUI_GetSliderValuei");
    GUI_SetSliderValue = (gui_set_slider_value_fn) so_symbol(&so_mod, "_Z18GUI_SetSliderValueif");

    for (int s = 0; s < REAL_SLOTS; s++)
        real[s].id = -1;

    sceTouchSetSamplingState(SCE_TOUCH_PORT_FRONT, SCE_TOUCH_SAMPLING_STATE_START);
    sceCtrlSetSamplingMode(SCE_CTRL_MODE_ANALOG_WIDE);

    ready = touchesBegan && touchesMoved && touchesEnded && touchesCancelled;
    if (!ready)
        l_error("input: IceAgeGLSurface natives missing, input disabled");
}

void input_update(void) {
    if (!ready)
        return;

    SceCtrlData pad;
    memset(&pad, 0, sizeof(pad));
    sceCtrlPeekBufferPositive(0, &pad, 1);
    uint32_t pressed = pad.buttons & ~old_buttons;
    old_buttons = pad.buttons;

    // Android back key: pause menu in game, previous page in menus.
    if ((pressed & SCE_CTRL_START) && nativeOnBackPressed) {
        jboolean wants_exit = nativeOnBackPressed(&jni, activity_obj);
        // true = main menu; Android would show "Exit?". Exiting is done from
        // the PS button on Vita, so this is only logged.
        if (wants_exit)
            l_info("input: back pressed at main menu (exit dialog skipped)");
    }

    update_real_touch();

    if (in_gameplay()) {
        menu_focused_ctl = -1;
        update_buttons(pad.buttons, pressed);
        update_move(axis(pad.lx), axis(pad.ly));
        update_look(axis(pad.rx), axis(pad.ry));
    } else {
        // Leaving gameplay (pause, statistics...): drop held synthetic touches.
        t_end(&move_touch);
        for (unsigned i = 0; i < NUM_BUTTONS; i++)
            t_end(&buttons[i].t);
        look_dx = look_dy = 0.0f;
        update_menu_navigation(pad.buttons, pressed, axis(pad.lx), axis(pad.ly));
    }

    // After this frame's touches: drop any release latch on the share buttons.
    hide_social_controls();
}

void input_release_all(void) {
    if (!ready)
        return;
    for (unsigned i = 0; i < NUM_BUTTONS; i++)
        t_end(&buttons[i].t);
    t_end(&move_touch);
    look_dx = look_dy = 0.0f;
    for (int s = 0; s < REAL_SLOTS; s++) {
        if (real[s].id != -1) {
            touchesEnded(&jni, surface_obj, real[s].x, real[s].y);
            real[s].id = -1;
        }
    }
    menu_focused_ctl = -1;
    nav_last_direction = 0;
    old_buttons = 0;
}
