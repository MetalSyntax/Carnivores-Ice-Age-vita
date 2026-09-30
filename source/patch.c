/*
 * Copyright (C) 2023 Volodymyr Atamanenko
 *
 * This software may be modified and distributed under the terms
 * of the MIT license. See the LICENSE file for details.
 */

/**
 * @file  patch.c
 * @brief Patching some of the .so internal functions or bridging them to native
 *        for better compatibility.
 */

#include <kubridge.h>
#include <so_util/so_util.h>

#include "input.h"
#include "reimpl/softfloat.h"
#include "utils/settings.h"

extern so_module so_mod;

void so_patch(void) {
    // Soft-float emulation -> VFP (the biggest CPU cost of an armeabi .so).
    if (setting_vfpFloat)
        softfloat_patch();

    // Right stick camera: feed the stick into the engine's own camera input.
    input_patch();
}
