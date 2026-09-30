/*
 * Copyright (C) 2022-2023 Volodymyr Atamanenko
 *
 * This software may be modified and distributed under the terms
 * of the MIT license. See the LICENSE file for details.
 */

/**
 * @file  settings.h
 * @brief Loader settings that can be set via a configurator app.
 */

#ifndef SOLOADER_SETTINGS_H
#define SOLOADER_SETTINGS_H

#include "stdbool.h"

#ifdef __cplusplus
extern "C" {
#endif

/** 0 = follow the Vita system language, 1 en, 2 de, 3 fr, 4 es. */
extern int  setting_language;
/** Report both Google Play content bundles as owned (no store on Vita). */
extern bool setting_unlockBundles;
/** Right analog stick camera speed, percent (10..400). */
extern int  setting_lookSensitivity;
extern bool setting_invertLookY;
/** Log the average frame rate every 5 seconds. */
extern bool setting_showFps;
extern int  setting_msaa;
extern bool setting_engineLog;
extern bool setting_vfpFloat;
/** In-game virtual buttons opacity, percent of the engine's own (0..100). */
extern int  setting_hudOpacity;

void settings_load();
void settings_save();
void settings_reset();

#ifdef __cplusplus
};
#endif

#endif // SOLOADER_SETTINGS_H
