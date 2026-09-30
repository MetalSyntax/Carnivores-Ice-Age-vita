# Carnivores: Ice Age — PS Vita port

so-loader port of the Android version (1.5.4) of *Carnivores: Ice Age* (Tatem Games / Action Forms).
It runs the original `libIceAgeAndroid.so` + `libfmodex.so` with an emulated Java layer (FalsoJNI).
You need your own copy of the game APK; nothing from the game is distributed here.

> Status: not yet tested on real hardware. See `port_progress.md`.

## Install

1. Install `kubridge.skprx` and `libshacccg.suprx` (ShaRKBR33D).
2. Install `carnivoresiceage.vpk`.
3. Create `ux0:data/carnivoresiceage/` and copy into it:
   - the original APK, renamed to `game.apk`
   - `libIceAgeAndroid.so` and `libfmodex.so` from the APK's `lib/armeabi/` folder
4. Launch. Logs go to `ux0:data/carnivoresiceage/logs/`.

## Controls

| Vita | Action |
|---|---|
| Touch screen | Original touch controls |
| Left stick | Move |
| Right stick | Look |
| R / Cross | Fire |
| L | Alternative fire |
| Square | Switch weapon |
| Triangle | Binoculars |
| D-pad up | Call |
| Select / D-pad down | Map |
| Start / Circle | Pause / back |

## Options

`ux0:data/carnivoresiceage/config.txt` (rewritten on every boot, so new keys show up):

| Key | Default | Meaning |
|---|---|---|
| `language` | `0` | 0 = system, 1 English, 2 German, 3 French, 4 Spanish |
| `unlock_bundles` | `1` | Treat the two Google Play content bundles as owned (there is no store on Vita) |
| `look_sensitivity` | `100` | Right stick camera speed in percent (10–400). Frame-rate independent; stacks with the in-game sensitivity slider |
| `invert_look_y` | `0` | Invert right stick vertical axis |
| `show_fps` | `0` | Log the frame rate and CPU (engine) / GPU (swap) time per frame every 5 seconds |
| `msaa` | `1` | Anti-aliasing: 0 off (fastest), 1 2x, 2 4x |
| `engine_log` | `0` | Write the game's own debug messages to the log (slow: it logs every frame) |
| `vfp_float` | `1` | Run the game's software floating point on the Vita FPU (big speedup; 0 only to rule it out if something looks wrong) |

## Building

Requires VitaSDK. vitaGL is vendored in `vendor/vitaGL` and built with `SOFTFP_ABI=1` automatically. Build and deploy with psvita-port-toolkit (`psvita-toolkit build`,
`psvita-toolkit deploy --vpk`), or plain CMake:

```sh
mkdir -p build && cd build && cmake .. && make
```

`extras/scripts/make_livearea.py` regenerates the LiveArea images from an extracted APK.

## Credits

Based on [soloader-boilerplate](https://github.com/v-atamanenko/soloader-boilerplate) (TheFloW, Rinnegatamante,
Volodymyr Atamanenko) and FalsoJNI.
