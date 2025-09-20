TRMNL Firmware — Project‑specific Developer Guidelines

Audience: advanced C++/embedded developers working on ESP32 (Arduino + ESP‑IDF via PlatformIO) and on host‑native unit tests.

Last verified with: PlatformIO Core 6.1.18 on 2025‑09‑19.

1) Build and configuration (firmware)

- Tooling
  - PlatformIO drives both Arduino and ESP‑IDF builds (dual framework). CMake/ESP‑IDF files are present but PlatformIO is the supported path for developer builds/flash.
  - Default PlatformIO environment is trmnl (see platformio.ini).

- Environments
  - Native (host tests): env:native, platform=native, Unity + ArduinoFake, C++11.
  - Device builds: extend env:esp32_base with board‑specific envs. Commonly used:
    - env:trmnl (ESP32‑C3, production board)
    - env:local (trmnl + USB CDC + extra logging conveniences)
    - Other boards: esp32dev, esp32-c3-devkitc-02, seeed_xiao_esp32c3, seeed_xiao_esp32s3, TRMNL_7inch5_OG_DIY_Kit, seeed_reTerminal_E1001, esp32-s3-devkitc-1
    - This fork specifically exists to support a custom PCB represented by the esp32-s3-devkitc-1 configuration

- Quick commands
  - Build: platformio run -e trmnl
  - Flash: platformio run -e trmnl -t upload
  - Serial monitor: platformio device monitor -e trmnl (115200, with esp32_exception_decoder)
  - Local dev env with USB CDC and wait‑for‑serial: platformio run -e local -t upload; then platformio device monitor -e local

- Configuration flags of interest
  - INCLUDE_GIT_HASH_IN_VERSION_NUMBER controls embedding git hash at build time (used by scripts/git_version.py). Pass via environment: INCLUDE_GIT_HASH_IN_VERSION_NUMBER=true platformio run -e trmnl
  - WAIT_FOR_SERIAL=1 is enabled in env:local to pause boot until a terminal attaches.
  - PNG_MAX_BUFFERED_PIXELS is tuned per env for PNGdec memory usage.
  - CORE_DEBUG_LEVEL is set to 0 by default; raise for verbose ESP‑IDF logging.

- Partitions and FS
  - Partition table: min_spiffs.csv
  - Filesystem: SPIFFS is enabled (board_build.filesystem = spiffs) for device logs/assets.

- SDK configs
  - sdkconfig.defaults and sdkconfig.trmnl (and variants) hold baseline Kconfig values per target. PlatformIO applies them via board_build.sdkconfig when present (see esp32-s3-devkitc-1).

2) Testing

- How tests are structured
  - All unit tests live under test/<suite_name> and are plain Unity programs ending in *.test.cpp with a main invoking UNITY_BEGIN()/RUN_TEST()/UNITY_END().
  - env:native provides ArduinoFake and defines -D ARDUINO=111 and -D ARDUINOJSON_ENABLE_ARDUINO_STRING=1.
  - Tests should avoid directly including Arduino.h in the native environment. Prefer ArduinoFake and project headers that are known to be portable. If a component requires Arduino types, wrap includes in guards like: #ifndef UNIT_TEST ... #endif or provide light stubs for native.

- Run all native tests (host)
  - platformio test -e native
  - Verified result on a clean run: 32 test cases across 8 suites passed in ~17s.

- Run a specific suite only
  - platformio test -e native -f test_string_utils
  - Other suite names include: test_bmp, test_png_flip, test_parse_api_display, test_parse_api_setup, test_serialize_log, test_serialize_api_log, test_stored_logs

- Add a new test (recommended pattern)
  1) Create a new folder: test/test_my_feature/
  2) Add a file test_my_feature.test.cpp with a Unity entry point. Example skeleton:

     #include <unity.h>
     // Prefer project headers that are portable to native
     void test_basic_math() { TEST_ASSERT_EQUAL_INT(2, 1 + 1); }
     int main(int, char**) { UNITY_BEGIN(); RUN_TEST(test_basic_math); return UNITY_END(); }

  3) Run just this suite while iterating: platformio test -e native -f test_my_feature

  Notes and pitfalls when adding tests:
  - The lib/ directory contains Arduino‑dependent code (includes Arduino.h). In env:native, PlatformIO’s dependency finder normally compiles only what’s referenced by the test. If your new test indirectly triggers compilation of lib/ sources that include Arduino.h, you will see build errors on host. To avoid this:
    - Keep your test’s includes limited to portable headers, or
    - Use ArduinoFake to satisfy Arduino symbols, or
    - Split Arduino‑dependent code behind small interfaces and provide a native stub in test code, or
    - Use UNIT_TEST preprocessor guards to exclude Arduino‑only sections from native builds.

- Creating integration/Arduino‑level tests
  - If you must include Arduino.h, use an ESP32 hardware env (e.g., -e local) and structure the test as application logic with assertions or serial‑reported results. PlatformIO’s native Unity runner is not used in that case.

3) Development/debugging tips

- Logging
  - Use trmnl_log.h macros. Serial‑only variants (Log_*_serial) avoid recursion and submission. Standard variants store locally for later submission; *_submit variants will attempt immediate submission (use with care in constrained stacks).
  - app_logger.cpp formats user messages with truncation and appends file:line; this reduces serial buffer allocations and makes logs traceable.
  - To route more logs to API/local store, raise the store_submit_threshold in app_logger.cpp as needed during debugging.

- Serial and boot behavior
  - For interactive bring‑up, use env:local which enables USB CDC and WAIT_FOR_SERIAL. This avoids missing early logs.
  - Exception decoding: monitor_filters = esp32_exception_decoder is enabled; capturing a crash in the serial monitor will be symbolicated.

- Code quality
  - clang-tidy is configured (check_tool = clangtidy) with bugprone, portability, clang‑analyzer, and google checks. Run: platformio check -e trmnl
  - C++ standard is gnu++11 in env:native; device builds inherit Arduino/ESP‑IDF toolchains via platform espressif32@6.11.0.

- Images/assets
  - PNGdec is used (pinned); PNG_MAX_BUFFERED_PIXELS is tuned per target. Test suites include PNG/BMP parsing and flips; see tests for reference usage.

- API and config
  - API serialization/parsing uses ArduinoJson@7.4.1. Tests cover both happy path and error handling for display/setup payloads.
  - sdkconfig.* variants and platformio env flags influence low‑level behavior (USB mode, CDC, PSRAM, flash mode/speed). For new hardware, prefer creating a new env that extends env:esp32_base and overrides only the required flags.

Appendix — Verified commands and outputs

- All host‑native tests: platformio test -e native
  - Observed: 8 suites, 32 cases: all passed.

- Filtered suite: platformio test -e native -f test_string_utils
  - Observed: suite passes and exercises string truncation helper used by logging.

Housekeeping
