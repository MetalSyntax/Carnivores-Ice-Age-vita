/*
 * Copyright (C) 2026 Carnivores Ice Age Vita port contributors
 *
 * This software may be modified and distributed under the terms
 * of the MIT license. See the LICENSE file for details.
 */

/**
 * @file  overlay.h
 * @brief Port UI drawn on top of the game with the engine's own fonts.
 *
 * Everything is in the engine's 2D GUI space: the same logical coordinates
 * as gui_controls[] (overlay_w() x overlay_h(), 480x320 on Vita), origin at
 * the bottom-left, y up. Colors are 0xAABBGGRR (byte order R, G, B, A, as the
 * engine's glColorPointer reads them).
 */

#ifndef CIA_OVERLAY_H
#define CIA_OVERLAY_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Font_PrintText() alignment flags. */
#define OVL_RIGHT   1
#define OVL_HCENTER 2
#define OVL_VCENTER 4

#define OVL_FONT       "ofs13"
#define OVL_FONT_TITLE "lith18"

/** Hook Font_Render() so the overlay is drawn over the whole frame. Called
 *  from so_patch(). */
void overlay_patch(void);

/** Called once per frame from inside Font_Render(); the only place where
 *  overlay_rect()/overlay_text() may be used. */
void overlay_set_callback(void (*cb)(void));

float overlay_w(void);
float overlay_h(void);

void overlay_rect(float x0, float y0, float x1, float y1, uint32_t abgr);
void overlay_frame(float x0, float y0, float x1, float y1, float t, uint32_t abgr);
void overlay_text(float x, float y, float scale, uint32_t abgr,
                  const char *text, uint32_t flags, const char *font);
/** Unscaled size of a text in GUI units (0 if the font is not loaded). */
void overlay_text_size(const char *text, const char *font, float *w, float *h);

#ifdef __cplusplus
};
#endif

#endif // CIA_OVERLAY_H
