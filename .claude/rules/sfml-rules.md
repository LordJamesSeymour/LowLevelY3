---
paths:
  - "**/*.cpp"
  - "**/*.h"
  - "**/*.hpp"
  - "CMakeLists.txt"
---

# SFML / C++ Rules

- This project uses C++17 and SFML 3.
- Check CMakeLists.txt before making build assumptions.
- Preserve existing class names and filenames.
- Prefer targeted fixes over full rewrites.
- If changing gameplay behaviour, check related UI, level editor, and reset logic.
- If changing input behaviour, check ArcadeInput first.
- If adding new UI text, check spacing, scaling, and controller navigation.
- Keep Raspberry Pi/Linux compatibility in mind.