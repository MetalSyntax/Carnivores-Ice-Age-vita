/*
 * Copyright (C) 2026 Carnivores Ice Age Vita port contributors
 *
 * This software may be modified and distributed under the terms
 * of the MIT license. See the LICENSE file for details.
 */

/**
 * @file  audio.h
 * @brief Native replacement for org.fmod.FMODAudioDevice (Java AudioTrack
 *        pump) feeding libfmodex.so's mixer into SceAudioOut.
 */

#ifndef SOLOADER_AUDIO_H
#define SOLOADER_AUDIO_H

#ifdef __cplusplus
extern "C" {
#endif

/** Start the pump thread (FMODAudioDevice.start()). Safe to call twice. */
void audio_start(void);

/** Stop the pump thread and release the audio port (FMODAudioDevice.stop()). */
void audio_stop(void);

/** Output silence without stopping FMOD (used while the app is suspended). */
void audio_set_muted(int muted);

#ifdef __cplusplus
};
#endif

#endif // SOLOADER_AUDIO_H
