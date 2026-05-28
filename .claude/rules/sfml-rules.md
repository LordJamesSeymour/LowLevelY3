---
paths:
  - "**/*.cpp"
  - "**/*.h"
  - "**/*.hpp"
  - "CMakeLists.txt"
---

# SFML / C++ Rules

## Core
- This project uses C++17 and SFML 3.
- Check CMakeLists.txt before making build assumptions.
- Preserve existing class names and filenames.
- Prefer targeted fixes over full rewrites.
- If changing gameplay behaviour, check related UI, level editor, and reset logic.
- If changing input behaviour, check ArcadeInput first.
- If adding new UI text, check spacing, scaling, and controller navigation.

## Linux / Raspberry Pi Portability — MANDATORY

Every change must compile and run on Raspberry Pi / Linux against system SFML 3 under `g++ -std=c++17`. Treat this as a hard requirement, not a nice-to-have.

- No unguarded Windows-only headers or APIs (`<windows.h>`, `<conio.h>`, `<direct.h>`, `MessageBox*`, `Sleep`, `GetTickCount`, `CreateThread`, `_getch`, `ShellExecute`, etc.). Wrap in `#if defined(_WIN32) ... #else ... #endif` with a working POSIX fallback.
- No Windows-only CRT: `fopen_s`, `sprintf_s`, `strcpy_s`, `_stricmp`, `_strdup`. Use C++ standard equivalents. Pair `localtime_s` with `localtime_r` under `_WIN32` guards.
- Asset paths are **case-sensitive** on Linux. Match exact on-disk casing for folders (`assets`, `Game#0`, `Bomberman`, `Resources`, `Maps`, `Shaders`) and extensions (`.png`, `.ttf`, `.wav`, `.ogg`, `.frag`). No `Assets/`, no `.PNG`.
- Forward slashes only in path literals. No `\\`. Compose paths with `std::filesystem::path` and the `/` operator.
- Use `std::filesystem` for all path manipulation. Paths are relative to the working directory.
- Standard library for OS facilities: `<thread>`, `<chrono>`, `<fstream>`, `std::this_thread::sleep_for`. No Win32 equivalents.
- Input via SFML enums / `ArcadeInput` only. No `VK_*`, `GetAsyncKeyState`.
- Text file parsers must tolerate both `\n` and `\r\n` (strip trailing `\r`).
- Use `<cstdint>` fixed-width types when serializing — do not assume `long` is 32 bits.
- No MSVC-only extensions: `__declspec`, `#pragma comment(lib, ...)`, `__forceinline`, `__int64`, `#pragma warning`. Use `[[nodiscard]]`, `inline`, `std::int64_t`.
- CMake glob picks up `.cpp` at repo root automatically; re-run `cmake -S . -B build` after adding files. Do not rename the executable target — it must stay `LLGP`.
- Vendored SFML 3.0.2 is Windows-only. Linux uses `find_package(SFML 3 ...)`.
- Windows-specific platform code lives only in `main.cpp` and `ArcadeHub.cpp`. Do not spread `#ifdef _WIN32` across gameplay/editor files.
- No `system()`, `popen()`, `ShellExecute`. No env-var lookups for config paths — config sits next to the working directory.

Before committing: "Will this compile on Raspberry Pi against system SFML 3?" If unsure, the change is not done.
