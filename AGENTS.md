# Repository Guidelines

## Project Structure & Module Organization
Firmware sources live in `src/` with shared headers in `include/` and reusable components under `lib/`. Hardware assets and reference binaries are in `builds/` and `pics/`; keep large artifacts out of Git and update `builds/` only when releasing. PlatformIO configuration resides in `platformio.ini`, while helper scripts (flash, merge, version stamping) live in `scripts/`—prefer extending those when adding workflows.

## Build, Test, and Development Commands
Use PlatformIO from the repo root. `pio run -e trmnl` builds the production target; switch `-e` to one of the other defined environments for board-specific builds. `pio run -e local -t upload` uploads over USB with serial logging enabled. Run unit tests with `pio test -e native`, which exercises the Unity suite without hardware. Lint C/C++ changes via `pio check -e trmnl` (clang-tidy). For quick flashing, the helper `scripts/flash_app.sh` wraps `esptool.py`; keep it updated when bootloader offsets change.

## Coding Style & Naming Conventions
Follow the existing Arduino/ESP-IDF hybrid style: two-space indentation, Allman braces, and trailing commas for multiline initialisers. Prefer snake_case for functions (`log_impl`), camelCase for locals and members, and UPPER_CASE for constants and macros. Keep headers self-contained, favour `#include <foo.h>` ordering from project to system, and use `Log_*` helpers instead of raw `Serial` prints to preserve structured logs.

## Testing Guidelines
Unit tests live in `test/<feature>/<name>.test.cpp` and rely on the Unity framework plus Arduino fakes. Mirror production data flows when adding fixtures and prefer deterministic inputs from `test/...` assets. Any bug fix or new API path should add or update a native test. When hardware behaviour must be validated manually, note the board, environment, and steps in the pull request.

## Commit & Pull Request Guidelines
Commit messages should stay in the imperative mood with concise context (`Fix guest mode refresh rate`). Group related changes per commit and avoid mixing formatting-only edits with behaviour changes. Pull requests need a problem statement, summary of changes, affected environments, and test evidence (`pio test -e native`, hardware screenshots where relevant). Link issues, tag reviewers who own the touched subsystems, and mention any deployment or flashing follow-up.
