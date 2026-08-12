# S2 Phase 10 Test Contract — Commercial packaging

> Written by the scrum-master BEFORE the Phase 10 engineer implements the feature.
> The engineer must implement the contract below so the tests pass. Do NOT weaken
> the assertions; implement the feature to satisfy them. This is a TEST CONTRACT.

## Build / run commands (HARD TIMEOUT, ONE AT A TIME)
```bash
cmake -S . -B build -DLODESTAR_BUILD_TESTS=ON
timeout 600 cmake --build build --config Release --target lodestar_s2_phase10_tests
timeout 120 ./build/core/Release/lodestar_s2_phase10_tests.exe
```

## Test file
- **File:** `core/test/s2_phase10_tests.cpp`
- **CMake target:** `lodestar_s2_phase10_tests`
- **Links:** `lodestar_common`, `lodestar_persistence`
- **Defines:** `LODESTAR_MIGRATIONS_DIR`
- **Binary:** `./build/core/Release/lodestar_s2_phase10_tests.exe`
- **Harness:** the same lightweight self-contained harness as WP-1..WP-8 / S1 phases.

## Contract the Phase 10 engineer must provide

### (A) License
- A `LICENSE` / `LICENSE.md` file exists describing the commercial license model.

### (B) Installer
- An installer script/config exists (e.g. `packaging/installer.ps1` or CPack config)
  that packages the built app + DLLs.

### (C) End-user docs
- `docs/user-guide.md` exists covering install, run, and main features.

### (D) Support model
- `docs/support.md` exists describing support tiers/contact.

## Test cases & expected behavior

### T1. license file exists
- `LICENSE` or `LICENSE.md` exists and is non-empty.

### T2. installer config exists
- An installer script/config exists (e.g. `packaging/installer.ps1` or CPack in
  `CMakeLists.txt`) and is non-empty.

### T3. user guide exists
- `docs/user-guide.md` exists and is non-empty.

### T4. support doc exists
- `docs/support.md` exists and is non-empty.

## CMake registration
Add inside the `if(LODESTAR_BUILD_TESTS)` block of `core/CMakeLists.txt`:
```cmake
add_executable(lodestar_s2_phase10_tests
    test/s2_phase10_tests.cpp)
target_link_libraries(lodestar_s2_phase10_tests PRIVATE
    lodestar_common
    lodestar_persistence)
target_compile_definitions(lodestar_s2_phase10_tests PRIVATE
    LODESTAR_MIGRATIONS_DIR="${CMAKE_CURRENT_SOURCE_DIR}/persistence/migrations")
```
