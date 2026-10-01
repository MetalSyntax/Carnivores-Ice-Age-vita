/*
 * Copyright (C) 2026 Carnivores Ice Age Vita port contributors
 *
 * This software may be modified and distributed under the terms
 * of the MIT license. See the LICENSE file for details.
 */

/**
 * @file  vita_menu.h
 * @brief In-game "PS Vita controls & camera" menu (button remapping, camera).
 */

#ifndef CIA_VITA_MENU_H
#define CIA_VITA_MENU_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

int  vita_menu_active(void);
void vita_menu_open(void);
/** Buttons of this frame (held / newly pressed); called by input_update()
 *  instead of feeding the game while the menu is open. */
void vita_menu_update(uint32_t held, uint32_t pressed);
/** Called from the overlay (inside Font_Render). */
void vita_menu_draw(void);

#ifdef __cplusplus
};
#endif

#endif // CIA_VITA_MENU_H
