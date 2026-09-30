# Carnivores: Ice Age — PS Vita port

A PS Vita port of the Android version (1.5.4) of *Carnivores: Ice Age* (Tatem Games).

It is a so-loader port: the original ARM game binaries from the Android APK (`libIceAgeAndroid.so` and its
audio engine `libfmodex.so`) run on the Vita, with the Android/Java side of the game reimplemented natively
(FalsoJNI), OpenGL ES 1.1 through vitaGL, and FMOD's Android audio output replaced by the Vita's audio ports.

**Nothing from the game is included.** You need your own copy of the Android APK.

## Features

- The full game runs: menus, hunting in every area you have the data for, weapons, calls, map, binoculars.
- Native physical controls (both sticks, buttons) plus the original touch controls.
- Right-stick camera driven directly into the game's camera code: fast, smooth and independent of the frame rate.
- The engine's software floating point (the APK is old `armeabi` code) runs on the Vita's FPU — both the game
  and FMOD. Together with an archive cache and a quieter log, the initial load went from ~31 s to ~17 s.
- Optional content packs (extra areas and weapons) are supported.
- Settings in a plain `config.txt`.

## Requirements

- A PS Vita / PS TV with HENkaku/Ensō (taiHEN).
- [kubridge](https://github.com/TheOfficialFloW/kubridge/releases) (`kubridge.skprx` in `ur0:tai/config.txt` under `*KERNEL`).
- `libshacccg.suprx` extracted to `ur0:data/` (use [ShaRKBR33D](https://github.com/Rinnegatamante/ShaRKBR33D)).
- The Android APK of *Carnivores: Ice Age* **1.5.4** (package `com.tatemgames.iceage`, `armeabi`).

## Installation

1. Install `carnivoresiceage.vpk` with VitaShell.
2. Create the folder `ux0:data/carnivoresiceage/` and copy into it:

   | File | Where it comes from |
   |---|---|
   | `game.apk` | The original APK, renamed |
   | `libIceAgeAndroid.so` | Inside the APK: `lib/armeabi/libIceAgeAndroid.so` |
   | `libfmodex.so` | Inside the APK: `lib/armeabi/libfmodex.so` |
   | `CarnivoresBundleOne.apk` *(optional)* | Content pack 1: areas 3–4 and the sniper rifle |
   | `CarnivoresBundleTwo.apk` *(optional)* | Content pack 2: area 6, double-barreled shotgun and crossbow |

   The content packs can also be named `bundle1.apk` / `bundle2.apk`, or be replaced by a Google Play
   expansion file named `main.obb` that contains both.
3. Launch the game from the LiveArea. The first boot creates `config.txt` and the `logs/` folder.

### About the weapons

The plain **shotgun** model is not in the 1.5.4 APK or in either content pack. The game still lets you pick it:
it keeps its own stats but is drawn with the double-barreled shotgun model (or the rifle if pack 2 is missing).
The same fallback applies to the pack 2 weapons (double-barreled shotgun, crossbow) and the pack 1 sniper rifle
when their pack is not installed. On Android the missing files froze the game; here they never do.

## Controls

| Vita | Action |
|---|---|
| Touch screen | Original touch controls (they all keep working, even when made transparent) |
| Left stick | Move |
| Right stick | Look around |
| R / Cross | Fire |
| L | Alternative fire |
| Square | Weapon button: draw the weapon and open / close the weapon list |
| Triangle | Binoculars |
| Circle / D-pad up | Call (animal call) |
| Select / D-pad down | Map |
| Start | Pause / back |

Buttons press the game's own on-screen controls, so they only act when that control is on screen.

## Options

`ux0:data/carnivoresiceage/config.txt` is rewritten on every boot, so keys added by newer versions show up
automatically. One `key value` per line:

| Key | Default | Meaning |
|---|---|---|
| `language` | `0` | 0 = system language, 1 English, 2 German, 3 French, 4 Spanish |
| `unlock_bundles` | `1` | Treat both content packs as purchased (there is no store on Vita) |
| `look_sensitivity` | `100` | Right stick camera speed in percent (10–400). Stacks with the in-game sensitivity slider |
| `invert_look_y` | `0` | Invert the right stick vertical axis |
| `msaa` | `1` | Anti-aliasing: 0 off (fastest), 1 = 2x, 2 = 4x |
| `show_fps` | `0` | Every 5 s, log the frame rate and the CPU (engine) / GPU (swap) time per frame |
| `engine_log` | `0` | Also log the game's own debug messages (slow: it logs every frame; for bug reports) |
| `hud_opacity` | `1` | Opacity of the in-game touch buttons, percent of the original (0 hidden, 100 original). They keep working when touched. The compass and the weapon/call lists are not affected |
| `vfp_float` | `1` | Run the game's and FMOD's software floating point on the FPU. Set to 0 only to rule it out if something looks wrong |

## Known issues

- Menus run at 60 FPS; 3D hunting scenes are much slower (16–19 FPS measured before the FPU patch, not
  re-measured since). `msaa 0` helps if the GPU is the limit — `show_fps 1` tells which side it is.
- The plain shotgun uses another weapon's model (see [About the weapons](#about-the-weapons)).
- Online features of the Android version (Facebook, Google Play Games, ads, in-app purchases) are stubbed out.
- The Android "Exit?" dialog is not shown: quit with the PS button.

## Reporting bugs

Attach the latest `ux0:data/carnivoresiceage/logs/carnivoresiceage_NNN.log`. For crashes also attach the
`.psp2dmp` from `ux0:data/`. Setting `engine_log 1` gives much more detail.

## Building

Requires [VitaSDK](https://vitasdk.org). vitaGL is vendored in `vendor/vitaGL` and is built automatically with
`SOFTFP_ABI=1` (the system `libvitaGL.a` gives a black screen with this port).

```sh
mkdir -p build && cd build
cmake ..
make
```

This produces `build/carnivoresiceage.vpk`. The port was developed with
psvita-port-toolkit (`psvita-toolkit build`, `psvita-toolkit deploy --vpk`); plain CMake works the same.
`extras/scripts/make_livearea.py` regenerates the LiveArea images from an extracted APK.

Technical notes (engine analysis, every patch applied to the `.so` and why) are in `PORTING_PLAN.md` and
`port_progress.md`.

## Credits

- Port: Wonder Diaz.
- [soloader-boilerplate](https://github.com/v-atamanenko/soloader-boilerplate) and
  [FalsoJNI](https://github.com/v-atamanenko/FalsoJNI) — Volodymyr Atamanenko, based on the so-loader work of
  Andy Nguyen (TheFloW) and Rinnegatamante.
- [vitaGL](https://github.com/Rinnegatamante/vitaGL) and [vitaShaRK](https://github.com/Rinnegatamante/vitaShaRK) — Rinnegatamante.
- [kubridge](https://github.com/TheOfficialFloW/kubridge) — TheFloW.
- *Carnivores: Ice Age* © Tatem Games. This project is not affiliated with or endorsed by Tatem Games.

## License

The loader code is MIT (see `LICENSE`). vitaGL (`vendor/vitaGL`) is LGPL-3.0. The game and FMOD are proprietary
and are not distributed with this project.
