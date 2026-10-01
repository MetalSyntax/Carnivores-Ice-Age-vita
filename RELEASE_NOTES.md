# Carnivores: Ice Age for PS Vita — v1.1

PS Vita port of *Carnivores: Ice Age* (Android 1.5.4). This version adds full menu navigation with the
buttons and the content packs from the game's Google Play expansion file.

**The game files are not included.** You need your own Android APK (1.5.4, `armeabi`) and, for zones 2–5, its
Google Play expansion file. Installation, controls and options are in the [README](README.md).

## Download

- `carnivoresiceage.vpk` — install with VitaShell (over v1.0 is fine, save data is kept).

Requires `kubridge.skprx` and `libshacccg.suprx` (ShaRKBR33D).

## What's new in v1.1

- **Menus with physical buttons.** D-Pad / left stick move a cursor drawn as four translucent white corners
  around each button, hunt cell or slider, without covering it; the focused button also shows its red "pressed" look. Cross selects, Circle goes back, Left / Right
  change sliders. Works in the main menus, the hunt setup pages and the in-game pause / statistics screens.
- **Content packs from the Google Play OBB.** Copy `main.33.com.tatemgames.iceage.obb` (or `main.obb`) to
  `ux0:data/carnivoresiceage/`, without extracting it:
  - pack 1: zones 2–3, shotgun, sniper rifle;
  - pack 2: zones 4–5, double-barreled shotgun, crossbow.
- **Fixed: purple sky and broken textures in zone 2 and later.** v1.0 unlocked every zone even when its files
  were missing, and the game loaded an empty terrain. Packs are now unlocked only when the OBB contains them;
  without it only the first zone can be selected.
- **Port menu to remap the controls** (Start + Select, or Select in the game's menus): every in-game action
  can be bound to any button (one or two each), plus camera speed, camera inversion (up/down, left/right),
  stick swap and touch HUD opacity. Saved in `controls.txt` / `config.txt`.
- **Photo mode on buttons**: R takes the photo, Right / Left zoom.
- **Facebook buttons hidden** (options menu, statistics and trophy screens): they did nothing on Vita.
- **Faster right-stick camera** with a finer response near the center.
- The `CarnivoresBundleOne/Two.apk` files mentioned in v1.0 belong to *Carnivores: Dinosaur Hunter* and do not
  work with this game; they can be deleted.

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
