/*
 * Copyright (C) 2026 Carnivores Ice Age Vita port contributors
 *
 * This software may be modified and distributed under the terms
 * of the MIT license. See the LICENSE file for details.
 */

#ifndef SOLOADER_SOFTFLOAT_H
#define SOLOADER_SOFTFLOAT_H

// Redirects the libgcc soft-float helpers inside libIceAgeAndroid.so to VFP.
// Must run after so_relocate() and before so_flush_caches().
void softfloat_patch(void);

#endif // SOLOADER_SOFTFLOAT_H
