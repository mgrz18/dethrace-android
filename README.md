# DethRace · Android port

[![Build APK](https://github.com/mgrz18/dethrace-android/actions/workflows/android.yml/badge.svg)](https://github.com/mgrz18/dethrace-android/actions/workflows/android.yml)

Native Android build of [DethRace](https://github.com/dethrace-labs/dethrace) — the clean-room reimplementation of the 1997 game **Carmageddon**. This fork takes the upstream `opengles` branch, wires it into a Gradle/NDK project, replaces the desktop OpenGL bits with OpenGL ES 3, and adds a touch overlay and lifecycle handling so the game is actually playable on a phone.

> **The desktop builds still work.** Every Android-specific change is gated behind `#ifdef __ANDROID__` or `if(ANDROID)` in CMake. Pull from `dethrace-labs/dethrace` upstream and you'll see no behavioural differences on Linux / macOS / Windows.

<p align="center">
  <em>Tested on Galaxy S25 Ultra (Adreno 830, Android 16) — should run on any Android 7+ device with GLES 3.</em>
</p>

---

## ⬇️ Just want to play?

You don't have to build anything. Each push to `android-port` produces a debug APK as a CI artifact:

1. Open <https://github.com/mgrz18/dethrace-android/actions/workflows/android.yml>
2. Click the most recent green run.
3. Scroll to **Artifacts** and download `dethrace-android-debug-apk`.
4. Unzip — inside is `app-debug.apk`.
5. Install it: `adb install -r app-debug.apk` *(or sideload from the file manager)*.
6. Provide the game data (next section).

> If you don't see Artifacts, you need to be signed in to GitHub.

---

## 🎮 Game data — required

DethRace ships **no game content**. You must own a copy of Carmageddon 1. The Max Pack from GoG is the easiest path:

- <https://www.gog.com/game/carmageddon_max_pack>

After installing it on a PC (or extracting the GoG installer with `innoextract` if you're on Mac/Linux), you'll have a folder layout like:

```
Carmageddon Max Pack/
├── CARMA/
│   └── DATA/                ← this folder
└── __support/
    └── app/
        └── CARMA/
            └── DATA/        ← these extras are critical (KEYMAP_*.TXT etc.)
```

Push them to your phone with `adb`:

```sh
# 1. The main DATA folder (~190 MB)
adb push "Carmageddon Max Pack/CARMA/DATA" \
    /sdcard/Android/data/com.dethrace.android/files/

# 2. The GoG support files (KEYMAP_*.TXT, OPTIONS.TXT, PATHS.TXT — tiny)
adb push "Carmageddon Max Pack/__support/app/CARMA/DATA/." \
    /sdcard/Android/data/com.dethrace.android/files/DATA/
```

Verify it landed where the game expects:

```sh
adb shell ls /sdcard/Android/data/com.dethrace.android/files/DATA/ | head
# CARS, RACES, SOUND, KEYMAP_0.TXT, ... = success
```

> **Splat Pack & demos:** also work, just push the corresponding `DATA/` from `CARSPLAT/` (Splat Pack) or `splatdem/`. The game auto-detects the installed mode.

---

## 🕹️ Controls

The game launches in landscape, fullscreen, immersive (no status bar / no nav bar). Touch overlay only appears when you're actually in a race — menus stay clean and respond to plain taps.

| Region | Action | Equivalent DOS key |
|---|---|---|
| Bottom-left, left disc ◀ | Steer left | Keypad 4 |
| Bottom-left, right disc ▶ | Steer right | Keypad 6 |
| Bottom-right, brake button | Brake / reverse | Keypad 2 |
| Bottom-right, gas button | Accelerate | Keypad 8 |
| Top-right, pause icon | Pause / open menu | Esc |

Touch zones are larger than the rendered icons (you can rest your thumbs in roughly the bottom-left and bottom-right corners — you don't have to aim at the icons). For weapons, lookback, etc., pair a Bluetooth keyboard.

---

## 🔧 Building from source

You only need to do this if you want to hack on the port. To just play, see *Just want to play?* above.

**Requirements**
- macOS or Linux (Windows untested but should work).
- Android Studio (or the standalone command-line tools) with:
  - SDK platform 34
  - NDK r26 (`26.1.10909125` is what CI uses)
  - CMake 3.22.1+
- Java 17.
- `curl`, `tar`, `unzip` (for the SDL2 download script).

**One-time setup**

```sh
git clone https://github.com/mgrz18/dethrace-android.git
cd dethrace-android

# Pull SDL2 source (~15 MB, extracts into android/app/jni/SDL/)
./android/scripts/setup-sdl.sh

# Tell Gradle where your Android SDK lives
echo "sdk.dir=$ANDROID_HOME" > android/local.properties
```

**Build**

```sh
cd android
./gradlew assembleDebug
```

The APK lands at `android/app/build/outputs/apk/debug/app-debug.apk`.

**Install + launch**

```sh
adb install -r android/app/build/outputs/apk/debug/app-debug.apk
adb shell am start -n com.dethrace.android/.DethraceActivity
```

To stream the game's logs:

```sh
adb logcat -s dethrace:V DethRace:V DethRace-native:V SDL:V
```

---

## 🧱 What changed from upstream

A short tour of the changes — full diff in the [first commit on `android-port`](https://github.com/mgrz18/dethrace-android/commits/android-port).

**New files**

| Path | Purpose |
|---|---|
| `android/` | Gradle project, Java code, build scripts, NDK glue |
| `android/app/src/main/java/com/dethrace/android/DethraceActivity.java` | Custom `SDLActivity`: forces landscape, sets `DETHRACE_ROOT_DIR`, immersive fullscreen, mounts the touch overlay, polls race state |
| `android/app/src/main/java/com/dethrace/android/TouchOverlayView.java` | Steering discs, gas/brake pedals and pause button drawn over the SDL surface |
| `android/scripts/setup-sdl.sh` | Downloads SDL2 2.30.10 source into `app/jni/SDL/` |
| `src/harness/os/android.c` | Bionic-friendly OS layer (signal handler, case-insensitive `OS_fopen`) |
| `src/DETHRACE/android_jni.c` | JNI bridge exposing `gProgram_state.racing` to Java |

**Engine tweaks** — all `#ifdef __ANDROID__` / `if(ANDROID)`:

- `harness_trace.c` — pipe `LOG_*` to `__android_log_print` so panics show up in `logcat` instead of disappearing into stdout.
- `gl_renderer.c` — skip `glReadPixels(GL_DEPTH_COMPONENT, ...)`. It's `GL_INVALID_OPERATION` on strict GLES drivers (Adreno) and was crashing on *Start Race*.
- `sdl_opengl.c` — go straight to GLES (no desktop-GL probe), load symbols with `gladLoadGLES2Loader`, cap to 60 fps via swap-interval 2 on 120 Hz panels, inject Keypad 4/6/8/2 + Esc from touch zones, pause `miniaudio` on `SDL_APP_*_BACKGROUND`.
- `harness.c` — `start_full_screen = 1` by default.
- `main.c` — include `<SDL.h>` so the `SDL_main` bridge picks it up.
- `CMakeLists.txt` patches — short-circuit `find_package(SDL2)` when SDL2 is already a target, build `dethrace` as `libmain.so`, drop pthread (in bionic), link `log android GLESv3 EGL`, swap `os/linux.c` for `os/android.c`.

---

## ⚠️ Limitations / known issues

- **`opengles` upstream branch is stale** — about 700 PRs behind `main`. Bug fixes landed there don't carry over yet.
- **Aspect ratio** — the engine renders at the original 320×200 (8:5). On 21:9 phones you'll see black pillars. Removing them while preserving the HUD requires non-trivial engine surgery (see *Future work*).
- **Limited input** — only steer/throttle/brake/menu. Weapons, lookback, handbrake etc. need an external keyboard or a future overlay extension.
- **Game files must be sideloaded** — for legal reasons we can't bundle Carmageddon assets.
- **No Splat Pack auto-merge yet** — push `CARSPLAT/DATA` separately if you want it.

---

## 🗺️ Future work

- Wider-FOV widescreen rendering (requires reallocating the BRender pixmap and HUD-coord remapping).
- Forward-port to upstream `main` to pick up ~700 PRs of fixes.
- Configurable touch layout, on-screen weapon buttons.
- Gamepad/keyboard remapping UI on the Android side.
- F-Droid / Play distribution once the port is more polished.

PRs welcome.

---

## 📜 License

Same as upstream DethRace. SDL2 is zlib. Carmageddon assets remain © Stainless Games — bring your own copy.
