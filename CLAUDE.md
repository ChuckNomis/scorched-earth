# Scorched Earth — Claude Instructions

## Project structure

- **`bagel26`** — CMake INTERFACE library (header-only ECS engine, `bagel.h`)
- **`se`** — CMake executable (the game). Build/run target in CLion: **se**

## CRITICAL: bagel.h is untouchable

Never modify `bagel.h` for any reason. It is the engine. SE must work within the engine's
constraints, not the other way around.

## CRITICAL: All ECS components must be trivially copyable

bagel's `DynamicBag` uses `malloc`/`realloc`. Assigning a non-trivially-copyable type
(e.g. `std::vector`, `std::string`) into a malloc'd slot is UB and crashes at runtime.

Every component must contain only plain data:
- ✅ `float`, `int`, `bool`, plain C arrays (`bool cells[50][8]`), `SDL_FRect`, `SDL_FPoint`
- ❌ `std::vector`, `std::string`, any type with a non-trivial copy constructor

## Files you may edit

- `SE/ScorchedEarth.h` — component definitions, storage specializations, class declaration
- `SE/ScorchedEarth.cpp` — all systems, factories, constructor/destructor/run()
- `main.cpp` — entry point
- `CMakeLists.txt` — build config
- `.planning/` — planning docs

## Files you must never edit

- `bagel.h` — engine, untouchable
- `lib/` — SDL submodules, untouchable
