# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project

This is my C++17 / SFML 3 low-level arcade project.

The project contains:

- ArcadeHub
- Super Bomberman
- Surfers Quest
- Level editors
- Level select screens
- Controller support
- Raspberry Pi / arcade cabinet target

## Build & Run

Two parallel build systems target the same source files. There are no automated tests — this is a game; verify changes by running the app.

**Windows (primary, Visual Studio 2022):**
- Solution: `LLGP.sln`, project file: `WindowsProject1.vcxproj`.
- Build x64 Debug/Release from the IDE, or from a Developer prompt:
  `msbuild LLGP.sln /p:Configuration=Debug /p:Platform=x64`
- SFML 3.0.2 is vendored in `SFML-3.0.2/`. The DLLs must sit next to the executable, and the `assets/` folder must be reachable from the working directory.

**Linux / Raspberry Pi (CMake):**
- `cmake -S . -B build && cmake --build build` produces the `LLGP` executable.
- `CMakeLists.txt` globs every `*.cpp` recursively (excluding `build/`, `x64/`, `Debug/`, `Release/`, and `ControllerTester.cpp`) and links `SFML::Graphics/Window/System/Audio` via `find_package(SFML 3 ...)` — SFML 3 must be installed on the system.
- Adding a new `.cpp` requires no CMake edits, but re-run CMake configure so the glob refreshes.

**Asset paths are relative to the working directory** (`std::filesystem::current_path()`), not the executable. Running from the wrong directory makes every `load()` fail. Error message boxes print the current working directory to help diagnose this.

## Architecture

**Single executable, single giant state machine.** `main.cpp` owns every screen and game as a stack-allocated object, loads them all up front, then runs one event/update/draw loop switched on an `AppState` enum. There is no scene-manager class — `main.cpp` *is* the orchestrator. State transitions go through the local `SetAppState` lambda, which also starts/stops per-screen music.

**Per-component contract.** Every screen/game class follows the same shape:
- `bool load(...)` returning false on failure, paired with `const std::string& getLastError()`. `main()` shows a Windows MessageBox and aborts if any `load` fails.
- `layout(window)` — recomputes positions for the current window size (called every frame; safe to call repeatedly).
- `update(deltaTime, ...)`, `draw(window)`.
- Input handlers: `handleKeyReleased(code)`, `handleClick(pos)`, `handleControllerInput()` returning an action enum.

**Input is split across two phases of the loop:**
- Keyboard & mouse are handled inside the `window.pollEvent()` loop, per `AppState`, via SFML events.
- Controller is polled *after* the event loop through the static `ArcadeInput` helper (see `ArcadeInput.h`), per `AppState`.
- After any state change or menu activation, call `ArcadeInput::consumePressedState()` so a held A/Start press doesn't immediately re-trigger on the next screen.

**Rendering & CRT shader.** Each state clears, draws, then calls the `DisplayFrame()` lambda, which (when `assets/Shaders/ArcadeHubCRT.frag` loaded) copies the framebuffer to a texture and re-draws it through a fragment shader for the CRT look. `ApplyWindowView` keeps a 1:1 view on resize.

### IMPORTANT: three different numbering schemes that DO NOT line up

This is the biggest source of confusion in the codebase. The hub position, the file prefix, and the asset folder use different numbers for the same game:

| Game (hub label)      | File prefix              | Gameplay classes        | Asset folder                  | `AppState`            |
|-----------------------|--------------------------|-------------------------|-------------------------------|-----------------------|
| GAME #1 — Bomberman   | `GAME1_Bomberman*`       | `Bomberman*` (Level/Player/Enemy/Bomb/Types) | `assets/Game#0/Bomberman` | `GAME1_Bomberman*`    |
| GAME #2 — Surfers Quest | `GAME1_*` (non-Bomberman) | `GAME1_Level/Player/Enemy/...` | `assets/Game#1/SurfersQuest` | `GAME1_Menu/Game/...` |
| GAME #3 — Space Shooter (locked) | `GAME2_*`     | `GAME2_*`               | `assets/Game#2`               | `GAME2_*`             |

So the `GAME1_` prefix is shared: `GAME1_Bomberman*` files belong to Bomberman, while the other `GAME1_*` files (`GAME1_Level`, `GAME1_Player`, `GAME1_Menu`, `GAME1_LevelEditor`, `GAME1_LevelSelect`, `GAME1_Enemy`, `GAME1_SurfersQuestAudio`) belong to **Surfers Quest**. Always confirm which game a file serves before editing. Bomberman's actual gameplay lives in `BombermanLevel/Player/Enemy/Bomb` driven by `GAME1_BombermanWindow`.

**Levels** are `assets/.../Maps/levelNN.txt` text files (validated by an exact `levelNN` + digits pattern). Each game has its own level editor and level-select screen.

## Important Instructions

- Preserve original filenames.
- Do not rename files with `_fixed`.
- Do not delete features unless I explicitly ask.
- Do not rewrite whole systems unless necessary.
- Inspect all relevant files before editing.
- Check both `.h` and `.cpp` files when changing a class.
- Use SFML 3 APIs, not SFML 2 assumptions.
- Be careful with asset paths and existing folder naming (note the `Game#0/#1/#2` vs hub numbering mismatch above).

## Linux / Raspberry Pi Portability — MANDATORY

Every line of code in this repository MUST compile and run on the Raspberry Pi / Linux target as well as Windows. This is non-negotiable. Before writing or editing code, assume it will be cross-compiled.

**Hard rules:**

1. **No unguarded Windows-only headers or APIs.** `<windows.h>`, `<conio.h>`, `<direct.h>`, `<tchar.h>`, `<shellapi.h>`, `MessageBox*`, `Sleep()`, `GetTickCount`, `CreateFileA`, `CreateThread`, `_getch`, `_kbhit`, `ShellExecute`, `OutputDebugString`, `WinMain`, `HWND`, `LPSTR`, etc. — ALL must be inside `#if defined(_WIN32) ... #else ... #endif` with a working POSIX fallback. No exceptions.
2. **No Windows-only CRT functions.** Do not use `fopen_s`, `sprintf_s`, `strcpy_s`, `strcat_s`, `localtime_s` alone, `_strdup`, `_stricmp`, `_snprintf`. Use the standard C++ equivalents (`std::ofstream`, `std::ostringstream`, `std::snprintf`, `strcasecmp` only inside POSIX guards, etc.). For `localtime`, pair `localtime_s` (Windows) with `localtime_r` (POSIX) under `_WIN32` guards — see `ArcadeHub.cpp:BuildCurrentClockText`.
3. **Paths are case-sensitive on Linux.** Every asset path string must EXACTLY match the on-disk casing — including folder names (`assets`, `Game#0`, `Bomberman`, `Resources`, `Maps`, `Shaders`) and file extensions (`.png`, `.ttf`, `.wav`, `.ogg`, `.frag`). Never write `Assets/`, `.PNG`, `Menu.ttf`, etc. Treat the actual filename on disk as the canonical form.
4. **Forward slashes only in path literals.** No `\\` in path strings. Prefer `std::filesystem::path` and the `/` operator to compose paths (`kRoot / "Maps" / "level01.txt"`) over raw string concatenation.
5. **Use `std::filesystem` for all path manipulation.** Do not assume drive letters, `\` separators, or `C:\`-style absolute paths. Asset paths are relative to `std::filesystem::current_path()`.
6. **Use the C++ standard library for OS facilities.** Threading → `<thread>` / `<mutex>`. Timing → `<chrono>`. File I/O → `<fstream>`. Process sleep → `std::this_thread::sleep_for`. No Win32 equivalents.
7. **Keyboard / input codes come from SFML enums only.** Never use Win32 `VK_*` codes, `GetAsyncKeyState`, `GetKeyState`. All input goes through SFML events or `ArcadeInput`.
8. **Line endings and text files.** Save level files (`levelNN.txt`) and shader files as UTF-8. Parsers must tolerate both `\n` and `\r\n` (strip trailing `\r` when reading lines).
9. **Endianness / integer sizes.** Do not assume `long` is 32 bits or that `int` and pointer sizes match. Use `<cstdint>` fixed-width types when serializing.
10. **Compiler-specific extensions banned.** No `__declspec`, `#pragma comment(lib, ...)`, `__forceinline`, `__int64`, MSVC-only `#pragma warning`. Use `[[nodiscard]]`, `[[maybe_unused]]`, `inline`, `std::int64_t`.
11. **CMake glob is authoritative for Linux builds.** Adding a `.cpp` at the repo root is picked up automatically; re-run `cmake -S . -B build` to refresh the glob. Do not place new sources under excluded paths (`build/`, `x64/`, `Debug/`, `Release/`). Do not rename the CMake executable target — it must stay `LLGP`.
12. **Vendored SFML is Windows-only.** The Linux build uses system-installed SFML 3 via `find_package(SFML 3 ...)`. Do not hardcode paths into `SFML-3.0.2/`.
13. **Windows-specific platform code lives only in `main.cpp`** (the `MessageBoxA` fatal-error popup) and `ArcadeHub.cpp` (`localtime_s` clock helper). If you must add more, isolate it behind `_WIN32` with a POSIX `#else` branch in the same translation unit. Do not spread platform `#ifdef` across gameplay/editor files.
14. **No `system()`, `popen()`, `ShellExecute`, or shelling out** for any reason — the arcade cabinet has no shell.
15. **No environment-variable assumptions.** Do not read `%APPDATA%`, `$HOME`, `$XDG_CONFIG_HOME`, etc. Config files (e.g. `settings.cfg`) live next to the working directory.

**Before committing any change, mentally answer:** "Will this compile with `g++ -std=c++17` on Raspberry Pi OS against system SFML 3?" If you cannot confidently say yes, the change is not done.

## Controller Rules

The project uses ArcadeInput for keyboard and controller input.

Current TRIXES/NES-style controller mapping:

- D-pad via joystick X/Y
- B = 0
- A = 1
- Select = 8
- Start = 9

Before changing controller behaviour, inspect ArcadeInput first.

## Working Process

Before making changes:

1. Read this CLAUDE.md.
2. Read the relevant Obsidian project note.
3. Read the latest checkpoint if available.
4. Inspect the relevant code files.
5. Explain the planned fix before editing, unless I ask for immediate code.

After making changes:

1. List changed files.
2. Explain the root cause.
3. Explain the fix.
4. Tell me how to test it.
5. Suggest a Git commit message.
6. If the fix is important, update Obsidian debugging history.

## Obsidian Vault

My project memory vault is here:

D:\ObsidianVaults\C++\LLGP\LowLevel-Y3

Important notes:

- `00_System/AI Agent Operating Rules.md`
- `01_Project_Index/C++ SFML Arcade.md`
- `03_Project_Rules/C++ SFML Rules.md`
- `02_Checkpoints/C++ SFML Arcade/`
- `04_Debugging_History/C++ SFML Arcade/`

When I ask for a checkpoint, write the checkpoint into:

`02_Checkpoints/C++ SFML Arcade/`

When I ask for a bug history note, write it into:

`04_Debugging_History/C++ SFML Arcade/`

## Response Style

- Keep responses short.
- Prefer code changes over explanations.
- After completing a task, respond with only:
  - changed files
  - completion status
  - test status
- Do not provide long explanations unless I explicitly ask.
