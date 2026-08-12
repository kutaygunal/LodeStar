# S2 Phase 12 Test Contract — OSLC integration

> Written by the scrum-master BEFORE the Phase 12 engineer implements the feature.
> The engineer must implement the contract below so the tests pass. Do NOT weaken
> the assertions; implement the feature to satisfy them. This is a TEST CONTRACT.

## Build / run commands (HARD TIMEOUT, ONE AT A TIME)
```bash
cmake -S . -B build -DLODESTAR_BUILD_TESTS=ON
timeout 600 cmake --build build --config Release --target lodestar_s2_phase12_tests
timeout 120 ./build/core/Release/lodestar_s2_phase12_tests.exe
```

## Test file
- **File:** `core/test/s2_phase12_tests.cpp`
- **CMake target:** `lodestar_s2_phase12_tests`
- **Links:** `lodestar_common`, `lodestar_persistence`, `lodestar_tracelink`
- **Defines:** `LODESTAR_MIGRATIONS_DIR`
- **Binary:** `./build/core/Release/lodestar_s2_phase12_tests.exe`
- **Harness:** the same lightweight self-contained harness as WP-1..WP-8 / S1 phases.

## Contract the Phase 12 engineer must provide

### (A) OSLC provider
- `exportRequirementAsOslc(requirementId)` produces an OSLC requirement resource
  (RDF/XML or JSON-LD) containing the requirement's identifier and title, using the
  standard OSLC RM namespaces (dcterms, oslc_rm).

### (B) OSLC consumer
- `importRequirementFromOslc(oslcResource)` parses an OSLC requirement resource and
  creates/updates a local requirement in the TraceLink model.

## Test cases & expected behavior

### T1. OSLC export contains identifier + title
- `exportRequirementAsOslc(reqId)` returns a string containing the requirement's
  identifier and title, and references the OSLC RM namespace (`oslc_rm`).

### T2. OSLC export is well-formed
- The exported resource contains `dcterms:identifier` and `dcterms:title` fields.

### T3. OSLC import creates a local requirement
- `importRequirementFromOslc(resource)` creates a requirement whose external id and
  title match the resource.

### T4. round-trip export→import preserves data
- Export a requirement, import it back, and the imported requirement has the same
  identifier and title.

## CMake registration
Add inside the `if(LODESTAR_BUILD_TESTS)` block of `core/CMakeLists.txt`:
```cmake
add_executable(lodestar_s2_phase12_tests
    test/s2_phase12_tests.cpp)
target_link_libraries(lodestar_s2_phase12_tests PRIVATE
    lodestar_common
    lodestar_persistence
    lodestar_tracelink)
target_compile_definitions(lodestar_s2_phase12_tests PRIVATE
    LODESTAR_MIGRATIONS_DIR="${CMAKE_CURRENT_SOURCE_DIR}/persistence/migrations")
```
