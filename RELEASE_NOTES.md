# Carnivores: Ice Age for PS Vita — v1.2

PS Vita port of *Carnivores: Ice Age* (Android 1.5.4). This version adds native PS Vita trophies and fixes the
menu cursor jumping the wrong way.

**The game files are not included.** You need your own Android APK (1.5.4, `armeabi`) and, for zones 2–5, its
Google Play expansion file. Installation, controls and options are in the [README](README.md).

## Download

- `carnivoresiceage.vpk` — install with VitaShell (over v1.1 is fine, save data is kept).

Requires `kubridge.skprx` and `libshacccg.suprx` (ShaRKBR33D). Trophies need `NoTrpDrm.suprx` under `*main`.

## What's new in v1.2

- **Native PS Vita trophies**: the game's 18 achievements (First Blood, Sharpshooter, Marathoner, Collector…)
  plus Platinum, with the system notification. Progress from v1.1 save data counts. Without NoTrpDrm the game
  runs as before with trophies off. On the first launch the screen stays black for a few seconds while the
  trophies install.
- **Fixed: menu cursor moving the wrong way.** Pressing Left / Right in a vertical list could jump to the
  button above or below. The cursor now only moves to buttons that are really in the pressed direction;
  Up / Down still reach corner buttons such as Back.

## What works

- Boot, menus, hunting in all five zones (with the OBB), weapons, calls, map, binoculars, save data.
- Physical controls for everything, menus included, plus the original touch controls.
- Audio (FMOD) through the Vita's audio output.

## Highlights

- **Right-stick camera** that feeds the game's own camera input: fast and independent of the frame rate
  (`look_sensitivity` in `config.txt`, 10–400 %).
- **FPU instead of software floating point** for both the game and FMOD (the APK is old `armeabi` code).
  Initial load: ~31 s → ~17 s.
- **No freeze on missing weapons**: without the OBB the shotgun has no model; such a weapon keeps its stats
  and uses another weapon's model instead of locking up the game.
- In-game touch buttons drawn at 1 % opacity (`hud_opacity`, 0–100) since every one has a physical button;
  the compass stays fully visible. Circle is the animal call.
- Settings in `ux0:data/carnivoresiceage/config.txt`: language, content packs, camera speed / inversion,
  anti-aliasing (`msaa`), FPS / timing log (`show_fps`).

## Known issues

- 3D scenes run well below 60 FPS (menus run at 60). Try `msaa 0`.
- Without the OBB only the first zone is available and the shotgun uses another weapon's model.
- Online features (Facebook, Google Play Games, ads, purchases) are disabled.
- No in-game exit dialog: quit with the PS button.

## Reporting bugs

Open an issue with `ux0:data/carnivoresiceage/logs/carnivoresiceage_NNN.log` (the newest one) and, for a crash,
the `.psp2dmp` file from `ux0:data/`. `engine_log 1` in `config.txt` adds the game's own messages.

## Credits

soloader-boilerplate and FalsoJNI (Volodymyr Atamanenko, after TheFloW and Rinnegatamante), vitaGL and vitaShaRK
(Rinnegatamante), kubridge (TheFloW). *Carnivores: Ice Age* © Tatem Games; this project is not affiliated with
Tatem Games.
