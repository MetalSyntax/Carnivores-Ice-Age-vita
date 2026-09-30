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

#ifdef __cplusplus
};
#endif

#endif // CIA_INPUT_H
