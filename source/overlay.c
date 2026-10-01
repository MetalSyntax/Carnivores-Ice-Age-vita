/*
 * Copyright (C) 2026 Carnivores Ice Age Vita port contributors
 *
 * This software may be modified and distributed under the terms
 * of the MIT license. See the LICENSE file for details.
 */

/**
 * @file  overlay.c
 * @brief Port UI drawn on top of the game with the engine's own fonts.
 *
 * Render() (pseudo-C ~8521 in Ice Age) ends the frame with Sprites_Render(),
 * Font_Render() and GUI_RenderFade(), all in the 2D GUI projection that
 * Render_Menu()/Render_Game() set up for GUI_DrawControls(). Font_PrintText()
 * only queues glyph quads per font; Font_Render() draws and empties the
 * queues. So the hook below lets the engine draw its own text first, then
 * draws the overlay's solid rectangles, queues the overlay text and runs
 * Font_Render() a second time: the overlay ends up above every menu label.
 *
 * GL state at that point (Render(), right before Sprites_Render): blend on
 * (SRC_ALPHA, ONE_MINUS_SRC_ALPHA), depth test and alpha test off, texture 2D
 * on. Font_Render() leaves the three client arrays disabled; the rectangles
 * put everything back the same way.
 */

#include "overlay.h"

#include "utils/logger.h"

#include <so_util/so_util.h>
#include <vitaGL.h>

#include <stddef.h>

extern so_module so_mod;

#define MAX_RECTS 96

static void (*Font_PrintText)(float x, float y, float scale, uint32_t color,
                              char *text, uint32_t flags, char *font);
static void (*Font_GetTextSize)(char *text, char *font, float *size);
static int *fonts_count;
// GUI space size. Not v_sx/v_sy as in Dinosaur Hunter: EAGLView() sets those
// to 960x640 here (hd_mode 1) while the GUI stays 480x320
// (createFramebuffer: scaleX = 480 / real_width, scaleY = 320 / real_height).
static float *p_scaleX, *p_scaleY;
static int *p_real_width, *p_real_height;

static so_hook font_render_hook;
static void (*callback)(void);

static float rect_v[MAX_RECTS * 12];
static uint32_t rect_c[MAX_RECTS * 6];
static int rect_count;
static int text_count;

float overlay_w(void) {
    if (p_scaleX && p_real_width && *p_scaleX > 0.0f && *p_real_width > 0)
        return *p_scaleX * (float) *p_real_width;
    return 480.0f;
}

float overlay_h(void) {
    if (p_scaleY && p_real_height && *p_scaleY > 0.0f && *p_real_height > 0)
        return *p_scaleY * (float) *p_real_height;
    return 320.0f;
}

void overlay_rect(float x0, float y0, float x1, float y1, uint32_t abgr) {
    if (rect_count >= MAX_RECTS)
        return;
    float *v = &rect_v[rect_count * 12];
    v[0] = x0; v[1]  = y0;  v[2] = x1;  v[3] = y0;  v[4]  = x1; v[5]  = y1;
    v[6] = x0; v[7]  = y0;  v[8] = x1;  v[9] = y1;  v[10] = x0; v[11] = y1;
    for (int i = 0; i < 6; i++)
        rect_c[rect_count * 6 + i] = abgr;
    rect_count++;
}

void overlay_frame(float x0, float y0, float x1, float y1, float t, uint32_t abgr) {
    overlay_rect(x0 - t, y0 - t, x1 + t, y0,     abgr);
    overlay_rect(x0 - t, y1,     x1 + t, y1 + t, abgr);
    overlay_rect(x0 - t, y0,     x0,     y1,     abgr);
    overlay_rect(x1,     y0,     x1 + t, y1,     abgr);
}

void overlay_text(float x, float y, float scale, uint32_t abgr,
                  const char *text, uint32_t flags, const char *font) {
    if (!Font_PrintText || !fonts_count || *fonts_count <= 0 || !text || !*text)
        return;
    Font_PrintText(x, y, scale, abgr, (char *) text, flags, (char *) font);
    text_count++;
}

void overlay_text_size(const char *text, const char *font, float *w, float *h) {
    float size[2] = {0.0f, 0.0f};
    if (Font_GetTextSize && fonts_count && *fonts_count > 0)
        Font_GetTextSize((char *) text, (char *) font, size);
    if (w) *w = size[0];
    if (h) *h = size[1];
}

void overlay_set_callback(void (*cb)(void)) {
    callback = cb;
}

static void draw_rects(void) {
    glDisable(GL_TEXTURE_2D);
    glEnableClientState(GL_VERTEX_ARRAY);
    glEnableClientState(GL_COLOR_ARRAY);
    glDisableClientState(GL_TEXTURE_COORD_ARRAY);
    glVertexPointer(2, GL_FLOAT, 0, rect_v);
    glColorPointer(4, GL_UNSIGNED_BYTE, 0, rect_c);
    glDrawArrays(GL_TRIANGLES, 0, rect_count * 6);
    glDisableClientState(GL_VERTEX_ARRAY);
    glDisableClientState(GL_COLOR_ARRAY);
    glEnable(GL_TEXTURE_2D);
}

static void Font_Render_hook(void) {
    SO_CONTINUE(int, font_render_hook);

    if (!callback)
        return;
    rect_count = 0;
    text_count = 0;
    callback();
    if (rect_count)
        draw_rects();
    if (text_count)
        SO_CONTINUE(int, font_render_hook);
}

void overlay_patch(void) {
    Font_PrintText   = (void *) so_symbol(&so_mod, "_Z14Font_PrintTextfffmPcmS_");
    Font_GetTextSize = (void *) so_symbol(&so_mod, "_Z16Font_GetTextSizePcS_P9_Vector2D");
    fonts_count      = (int *) so_symbol(&so_mod, "fonts_count");
    p_scaleX         = (float *) so_symbol(&so_mod, "scaleX");
    p_scaleY         = (float *) so_symbol(&so_mod, "scaleY");
    p_real_width     = (int *) so_symbol(&so_mod, "real_width");
    p_real_height    = (int *) so_symbol(&so_mod, "real_height");
    uintptr_t fn = so_symbol(&so_mod, "_Z11Font_Renderv");
    if (!Font_PrintText || !Font_GetTextSize || !fonts_count || !fn) {
        l_error("overlay: Font_Render not hooked, port menu will be invisible");
        return;
    }
    font_render_hook = hook_addr(fn, (uintptr_t) &Font_Render_hook);
}
