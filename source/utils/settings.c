/*
 * Copyright (C) 2021      Andy Nguyen
 * Copyright (C) 2022-2023 Volodymyr Atamanenko
 *
 * This software may be modified and distributed under the terms
 * of the MIT license. See the LICENSE file for details.
 */

#include <stdio.h>
#include <string.h>
#include "settings.h"

#define CONFIG_FILE_PATH DATA_PATH"config.txt"

int  setting_language;
bool setting_unlockBundles;
int  setting_lookSensitivity;
bool setting_invertLookY;
bool setting_showFps;
int  setting_msaa;
bool setting_engineLog;
bool setting_vfpFloat;

void settings_reset() {
    setting_language        = 0;     // 0 = system language, 1 en, 2 de, 3 fr, 4 es
    setting_unlockBundles   = true;  // Google Play bundles cannot be bought on Vita
    setting_lookSensitivity = 100;   // percent, right analog stick camera speed (100 = 300 logical px/s)
    setting_invertLookY     = false;
    setting_showFps         = false;
    setting_msaa            = 1;     // 0 off, 1 2x, 2 4x (GPU cost at 960x544)
    setting_engineLog       = false; // engine __android_log_* spam -> log file
    setting_vfpFloat        = true;  // run the engine's soft-float math on the VFP
}

void settings_load() {
    settings_reset();

    char buffer[64];
    int value;

    FILE *config = fopen(CONFIG_FILE_PATH, "r");

    if (config) {
        while (EOF != fscanf(config, "%63[^ ] %d\n", buffer, &value)) {
            if      (strcmp("language", buffer) == 0)         setting_language        = value;
            else if (strcmp("unlock_bundles", buffer) == 0)   setting_unlockBundles   = (bool)value;
            else if (strcmp("look_sensitivity", buffer) == 0) setting_lookSensitivity = value;
            else if (strcmp("invert_look_y", buffer) == 0)    setting_invertLookY     = (bool)value;
            else if (strcmp("show_fps", buffer) == 0)         setting_showFps         = (bool)value;
            else if (strcmp("msaa", buffer) == 0)             setting_msaa            = value;
            else if (strcmp("engine_log", buffer) == 0)       setting_engineLog       = (bool)value;
            else if (strcmp("vfp_float", buffer) == 0)        setting_vfpFloat        = (bool)value;
        }
        fclose(config);
    }

    if (setting_language < 0 || setting_language > 4) setting_language = 0;
    if (setting_lookSensitivity < 10) setting_lookSensitivity = 10;
    if (setting_lookSensitivity > 400) setting_lookSensitivity = 400;
    if (setting_msaa < 0 || setting_msaa > 2) setting_msaa = 1;

    // Always rewrite: writes the defaults on first boot and adds keys that
    // are new in this version to an existing config.txt.
    settings_save();
}

void settings_save() {
    FILE *config = fopen(CONFIG_FILE_PATH, "w+");

    if (config) {
        fprintf(config, "%s %d\n", "language", setting_language);
        fprintf(config, "%s %d\n", "unlock_bundles", (int)setting_unlockBundles);
        fprintf(config, "%s %d\n", "look_sensitivity", setting_lookSensitivity);
        fprintf(config, "%s %d\n", "invert_look_y", (int)setting_invertLookY);
        fprintf(config, "%s %d\n", "show_fps", (int)setting_showFps);
        fprintf(config, "%s %d\n", "msaa", setting_msaa);
        fprintf(config, "%s %d\n", "engine_log", (int)setting_engineLog);
        fprintf(config, "%s %d\n", "vfp_float", (int)setting_vfpFloat);
        fclose(config);
    }
}
