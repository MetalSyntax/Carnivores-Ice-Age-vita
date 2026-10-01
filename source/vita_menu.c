/*
 * Copyright (C) 2026 Carnivores Ice Age Vita port contributors
 *
 * This software may be modified and distributed under the terms
 * of the MIT license. See the LICENSE file for details.
 */

/**
 * @file  vita_menu.c
 * @brief In-game "PS Vita controls & camera" menu (button remapping, camera).
 *
 * Opened with START + SELECT (SELECT alone in the game's menus, see
 * input_update()). In game the hunt is paused first through the engine's own
 * back key, so nothing moves underneath. Saved to controls.txt and
 * config.txt when closed.
 */

#include "vita_menu.h"

#include "input.h"
#include "overlay.h"
#include "utils/settings.h"

#include <psp2/ctrl.h>
#include <psp2/kernel/processmgr.h>

#include <stdio.h>

typedef enum {
    ROW_ACTION,
    ROW_LOOK_SPEED,
    ROW_INVERT_Y,
    ROW_INVERT_X,
    ROW_SWAP_STICKS,
    ROW_HUD_OPACITY,
    ROW_DEFAULTS,
    ROW_CLOSE,
} RowKind;

#define MAX_ROWS 32

#define COL_WHITE   0xffffffff
#define COL_GREY    0xffb4b4b4
#define COL_ACCENT  0xff00c8ff
#define COL_SEL_BG  0x5000c8ff
#define COL_DIM     0xb0000000
#define COL_PANEL   0xf0141a1e
#define COL_BORDER  0xff4a6070

#define REPEAT_DELAY 400000
#define REPEAT_RATE  110000

static const int hud_steps[] = { 0, 1, 5, 10, 20, 30, 40, 50, 60, 70, 80, 90, 100 };
#define HUD_STEPS (int) (sizeof(hud_steps) / sizeof(hud_steps[0]))

static struct {
    RowKind kind;
    int action;
} rows[MAX_ROWS];
static int row_count;

static int open;
static int sel, scroll;
static int capture;             // 0 off, 1 replace, 2 add
static uint32_t prev_dirs;
static uint64_t repeat_at;

static void build_rows(void) {
    row_count = 0;
    for (int i = 0; i < input_action_count() && row_count < MAX_ROWS - 7; i++) {
        rows[row_count].kind = ROW_ACTION;
        rows[row_count++].action = i;
    }
    rows[row_count++].kind = ROW_LOOK_SPEED;
    rows[row_count++].kind = ROW_INVERT_Y;
    rows[row_count++].kind = ROW_INVERT_X;
    rows[row_count++].kind = ROW_SWAP_STICKS;
    rows[row_count++].kind = ROW_HUD_OPACITY;
    rows[row_count++].kind = ROW_DEFAULTS;
    rows[row_count++].kind = ROW_CLOSE;
}

int vita_menu_active(void) {
    return open;
}

void vita_menu_open(void) {
    build_rows();
    open = 1;
    capture = 0;
    prev_dirs = SCE_CTRL_UP | SCE_CTRL_DOWN | SCE_CTRL_LEFT | SCE_CTRL_RIGHT;
    if (sel < 0 || sel >= row_count)
        sel = 0;
}

static void close_menu(void) {
    settings_save();
    input_controls_save();
    open = 0;
    capture = 0;
}

static int clampi(int v, int lo, int hi) { return v < lo ? lo : (v > hi ? hi : v); }

static void hud_step(int delta) {
    int i = 0;
    while (i < HUD_STEPS - 1 && hud_steps[i] < setting_hudOpacity)
        i++;
    setting_hudOpacity = hud_steps[clampi(i + delta, 0, HUD_STEPS - 1)];
}

static void change_value(RowKind kind, int delta) {
    switch (kind) {
    case ROW_LOOK_SPEED:
        setting_lookSensitivity = clampi(setting_lookSensitivity + delta * 10, 10, 400);
        break;
    case ROW_INVERT_Y:    setting_invertLookY = !setting_invertLookY; break;
    case ROW_INVERT_X:    setting_invertLookX = !setting_invertLookX; break;
    case ROW_SWAP_STICKS: setting_swapSticks = !setting_swapSticks;   break;
    case ROW_HUD_OPACITY: hud_step(delta); break;
    default: break;
    }
}

static void restore_defaults(void) {
    input_controls_defaults();
    setting_lookSensitivity = 100;
    setting_invertLookY = false;
    setting_invertLookX = false;
    setting_swapSticks = false;
    setting_hudOpacity = 1;
}

void vita_menu_update(uint32_t held, uint32_t pressed) {
    if (!open)
        return;

    if (capture) {
        if (pressed & SCE_CTRL_START) {
            capture = 0;
            return;
        }
        uint32_t b = pressed & input_bindable_buttons();
        if (b) {
            b &= -b; // one button per press
            input_action_bind(rows[sel].action, b, capture == 2);
            capture = 0;
        }
        return;
    }

    if (pressed & (SCE_CTRL_CIRCLE | SCE_CTRL_START)) {
        close_menu();
        return;
    }

    uint32_t dirs = held & (SCE_CTRL_UP | SCE_CTRL_DOWN | SCE_CTRL_LEFT | SCE_CTRL_RIGHT);
    uint32_t fresh = dirs & ~prev_dirs;
    prev_dirs = dirs;
    uint64_t now = sceKernelGetProcessTimeWide();
    uint32_t step = 0;
    if (fresh) {
        step = fresh;
        repeat_at = now + REPEAT_DELAY;
    } else if (dirs && now >= repeat_at) {
        step = dirs;
        repeat_at = now + REPEAT_RATE;
    }

    if (step & SCE_CTRL_UP)
        sel = (sel + row_count - 1) % row_count;
    else if (step & SCE_CTRL_DOWN)
        sel = (sel + 1) % row_count;

    RowKind kind = rows[sel].kind;
    switch (kind) {
    case ROW_ACTION:
        if (pressed & SCE_CTRL_CROSS)
            capture = 1;
        else if (pressed & SCE_CTRL_SQUARE)
            capture = 2;
        else if (pressed & SCE_CTRL_TRIANGLE)
            input_action_clear(rows[sel].action);
        break;
    case ROW_DEFAULTS:
        if (pressed & SCE_CTRL_CROSS)
            restore_defaults();
        break;
    case ROW_CLOSE:
        if (pressed & SCE_CTRL_CROSS)
            close_menu();
        break;
    case ROW_LOOK_SPEED:
    case ROW_HUD_OPACITY:
        // Only the d-pad repeats; toggles below react to fresh presses.
        if (step & SCE_CTRL_LEFT)
            change_value(kind, -1);
        else if (step & (SCE_CTRL_RIGHT))
            change_value(kind, 1);
        else if (pressed & SCE_CTRL_CROSS)
            change_value(kind, 1);
        break;
    default:
        if ((fresh & (SCE_CTRL_LEFT | SCE_CTRL_RIGHT)) || (pressed & SCE_CTRL_CROSS))
            change_value(kind, 1);
        break;
    }
}

/* --- drawing -------------------------------------------------------------- */

static const char *row_label(int r) {
    switch (rows[r].kind) {
    case ROW_ACTION:      return input_action_label(rows[r].action);
    case ROW_LOOK_SPEED:  return "Camera speed (right stick)";
    case ROW_INVERT_Y:    return "Invert camera up/down";
    case ROW_INVERT_X:    return "Invert camera left/right";
    case ROW_SWAP_STICKS: return "Swap sticks (left = camera)";
    case ROW_HUD_OPACITY: return "Touch HUD opacity";
    case ROW_DEFAULTS:    return "Restore defaults";
    case ROW_CLOSE:       return "Save and close";
    }
    return "";
}

static void row_value(int r, char *out, size_t size) {
    out[0] = '\0';
    switch (rows[r].kind) {
    case ROW_ACTION:
        if (capture && r == sel)
            snprintf(out, size, "%s", capture == 2 ? "add: press a button..." : "press a button...");
        else
            input_buttons_text(input_action_buttons(rows[r].action), out, size);
        break;
    case ROW_LOOK_SPEED:  snprintf(out, size, "< %d%% >", setting_lookSensitivity); break;
    case ROW_INVERT_Y:    snprintf(out, size, "%s", setting_invertLookY ? "On" : "Off"); break;
    case ROW_INVERT_X:    snprintf(out, size, "%s", setting_invertLookX ? "On" : "Off"); break;
    case ROW_SWAP_STICKS: snprintf(out, size, "%s", setting_swapSticks ? "On" : "Off"); break;
    case ROW_HUD_OPACITY: snprintf(out, size, "< %d%% >", setting_hudOpacity); break;
    default: break;
    }
}

static const char *footer(void) {
    if (capture)
        return "Press the new button    START: cancel";
    switch (rows[sel].kind) {
    case ROW_ACTION:
        return "Cross: set   Square: add   Triangle: clear   Circle: save";
    case ROW_LOOK_SPEED:
    case ROW_HUD_OPACITY:
        return "Left / Right: change    Circle: save and close";
    case ROW_INVERT_Y:
    case ROW_INVERT_X:
    case ROW_SWAP_STICKS:
        return "Cross: toggle    Circle: save and close";
    default:
        return "Cross: select    Circle: save and close";
    }
}

void vita_menu_draw(void) {
    if (!open)
        return;

    float w = overlay_w(), h = overlay_h();
    float px0 = 24.0f, px1 = w - 24.0f, py0 = 8.0f, py1 = h - 8.0f;

    overlay_rect(0.0f, 0.0f, w, h, COL_DIM);
    overlay_rect(px0, py0, px1, py1, COL_PANEL);
    overlay_frame(px0, py0, px1, py1, 1.0f, COL_BORDER);

    float th;
    overlay_text_size("Ag", OVL_FONT, NULL, &th);
    float row_h = 15.0f;
    float scale = th > 0.0f && th > row_h - 2.0f ? (row_h - 2.0f) / th : 1.0f;

    overlay_text((px0 + px1) * 0.5f, py1 - 14.0f, 0.75f, COL_ACCENT,
                 "PS Vita controls & camera", OVL_HCENTER | OVL_VCENTER, OVL_FONT_TITLE);
    overlay_rect(px0 + 10.0f, py1 - 27.0f, px1 - 10.0f, py1 - 26.0f, COL_BORDER);

    float list_top = py1 - 30.0f;
    float list_bottom = py0 + 22.0f;
    int visible = (int) ((list_top - list_bottom) / row_h);
    if (visible < 1)
        visible = 1;
    if (sel < scroll)
        scroll = sel;
    if (sel >= scroll + visible)
        scroll = sel - visible + 1;
    scroll = clampi(scroll, 0, row_count > visible ? row_count - visible : 0);

    float value_x = px0 + (px1 - px0) * 0.52f;
    for (int i = 0; i < visible && scroll + i < row_count; i++) {
        int r = scroll + i;
        float top = list_top - i * row_h;
        float cy = top - row_h * 0.5f;

        // Gap before the camera block.
        if (rows[r].kind != ROW_ACTION && r > 0 && rows[r - 1].kind == ROW_ACTION)
            overlay_rect(px0 + 10.0f, top, px1 - 10.0f, top + 0.5f, COL_BORDER);

        if (r == sel)
            overlay_rect(px0 + 6.0f, top - row_h + 1.0f, px1 - 6.0f, top, COL_SEL_BG);

        uint32_t col = r == sel ? COL_WHITE : COL_GREY;
        overlay_text(px0 + 14.0f, cy, scale, col, row_label(r), OVL_VCENTER, OVL_FONT);

        char value[96];
        row_value(r, value, sizeof(value));
        overlay_text(value_x, cy, scale, r == sel ? COL_ACCENT : COL_WHITE, value,
                     OVL_VCENTER, OVL_FONT);
    }

    // Scroll marks.
    if (scroll > 0)
        overlay_text(px1 - 12.0f, list_top - 4.0f, scale, COL_GREY, "^", OVL_RIGHT | OVL_VCENTER, OVL_FONT);
    if (scroll + visible < row_count)
        overlay_text(px1 - 12.0f, list_bottom + 4.0f, scale, COL_GREY, "v", OVL_RIGHT | OVL_VCENTER, OVL_FONT);

    overlay_rect(px0 + 10.0f, py0 + 18.0f, px1 - 10.0f, py0 + 19.0f, COL_BORDER);
    overlay_text((px0 + px1) * 0.5f, py0 + 9.0f, scale * 0.9f, COL_GREY, footer(),
                 OVL_HCENTER | OVL_VCENTER, OVL_FONT);
}
