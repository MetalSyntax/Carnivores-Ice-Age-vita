# Carnivores: Ice Age for PS Vita — v1.0

First release of the PS Vita port of *Carnivores: Ice Age* (Android 1.5.4).

**The game files are not included.** You need your own Android APK (1.5.4, `armeabi`). Installation, controls
and options are in the [README](README.md).

## Download

- `carnivoresiceage.vpk` — install with VitaShell.

Requires `kubridge.skprx` and `libshacccg.suprx` (ShaRKBR33D).

## What works

- Boot, menus, hunting, weapons, calls, map, binoculars, save data.
- Physical controls for everything, plus the original touch controls.
- Audio (FMOD) through the Vita's audio output.
- Optional content packs: `CarnivoresBundleOne.apk` (areas 3–4, sniper rifle) and `CarnivoresBundleTwo.apk`
  (area 6, double-barreled shotgun, crossbow), or a Google Play `main.obb`.

## Highlights

- **Right-stick camera** that feeds the game's own camera input: fast and independent of the frame rate
  (`look_sensitivity` in `config.txt`, 10–400 %).
- **FPU instead of software floating point** for both the game and FMOD (the APK is old `armeabi` code).
  Initial load: ~31 s → ~17 s.
- **No freeze on missing weapons.** The plain shotgun model is in none of the 1.5.4 files; the game used to
  lock up the first time a weapon without files was drawn. Such a weapon now keeps its stats and uses another
  weapon's model.
- In-game touch buttons drawn at 1 % opacity (`hud_opacity`, 0–100) since every one has a physical button;
  the compass stays fully visible. Circle is the animal call.
- Settings in `ux0:data/carnivoresiceage/config.txt`: language, content packs, camera speed / inversion,
  anti-aliasing (`msaa`), FPS / timing log (`show_fps`).

## Known issues

- 3D scenes run well below 60 FPS (menus run at 60). Try `msaa 0`.
- The plain shotgun is drawn with the double-barreled shotgun model (or the rifle without pack 2).
- Online features (Facebook, Google Play Games, ads, purchases) are disabled.
- No in-game exit dialog: quit with the PS button.

## Reporting bugs

Open an issue with `ux0:data/carnivoresiceage/logs/carnivoresiceage_NNN.log` (the newest one) and, for a crash,
the `.psp2dmp` file from `ux0:data/`. `engine_log 1` in `config.txt` adds the game's own messages.

## Credits

soloader-boilerplate and FalsoJNI (Volodymyr Atamanenko, after TheFloW and Rinnegatamante), vitaGL and vitaShaRK
(Rinnegatamante), kubridge (TheFloW). *Carnivores: Ice Age* © Tatem Games; this project is not affiliated with
Tatem Games.
