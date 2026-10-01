/*
 * Copyright (C) 2026 Carnivores Ice Age Vita port contributors
 *
 * This software may be modified and distributed under the terms
 * of the MIT license. See the LICENSE file for details.
 */

/**
 * @file  input.h
 * @brief Front touch + physical controls -> IceAgeGLSurface touches.
 */

#ifndef CIA_INPUT_H
#define CIA_INPUT_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Hook the engine's camera input (GUI_GetBackgroundMovements) so the right
 *  stick drives the camera directly. Called from so_patch(). */
void input_patch(void);

/** Resolve the engine symbols used by the mapper and start touch sampling. */
void input_init(void);

/** Poll touch/buttons and forward them to the engine. Call once per frame
 *  on the render thread, before layoutSubviews(). */
void input_update(void);

/** Release every active (real and synthetic) touch, e.g. before suspending. */
void input_release_all(void);

/** Reload the button bindings from controls.txt (written with the defaults
 *  if missing). */
void input_reload_controls(void);

/* --- bindings, used by the port menu (vita_menu.c) ---------------------- */

int         input_action_count(void);
const char *input_action_label(int action);
uint32_t    input_action_buttons(int action);
/** Bind `button` (one bit) to `action`, removing it from every other action.
 *  `add` keeps the buttons the action already had. */
void        input_action_bind(int action, uint32_t button, int add);
void        input_action_clear(int action);
void        input_controls_defaults(void);
void        input_controls_save(void);
/** Buttons that can be bound (everything but START). */
uint32_t    input_bindable_buttons(void);
/** "R, Cross" style list of the buttons in `mask` ("-" if none). */
void        input_buttons_text(uint32_t mask, char *out, size_t size);

#ifdef __cplusplus
};
#endif

#endif // CIA_INPUT_H
