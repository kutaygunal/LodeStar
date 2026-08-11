# core/CMakeLists.txt
# C++ service core. Each module is a static library target.

include(LodestarUtils)

# --- SQLite via vcpkg ------------------------------------------------------
find_package(unofficial-sqlite3 CONFIG REQUIRED)

# --- common: logging, config, utils ----------------------------------------
lodestar_add_module(lodestar_common
    common/Logger.cpp
    common/Config.cpp
    common/Uuid.cpp
    common/Version.cpp)
target_compile_definitions(lodestar_common PRIVATE _CRT_SECURE_NO_WARNINGS)

# --- persistence: SQLite schema + DAOs + migration runner ------------------
lodestar_add_module(lodestar_persistence
    persistence/Database.cpp
    persistence/MigrationRunner.cpp
    persistence/daos.cpp)
target_link_libraries(lodestar_persistence PUBLIC lodestar_common)
target_link_libraries(lodestar_persistence PUBLIC unofficial::sqlite3::sqlite3)
target_compile_definitions(lodestar_persistence PRIVATE
    LODESTAR_MIGRATIONS_DIR="${CMAKE_CURRENT_SOURCE_DIR}/persistence/migrations")

# --- tracelink: graph model backed by persistence --------------------------
lodestar_add_module(lodestar_tracelink
    tracelink/TraceGraph.cpp)
target_link_libraries(lodestar_tracelink PUBLIC lodestar_common lodestar_persistence)

# --- Stub modules (empty libraries that compile) ----------------------------
lodestar_add_module(lodestar_scenario      scenario/stub.cpp)
lodestar_add_module(lodestar_testforge     testforge/stub.cpp)
lodestar_add_module(lodestar_assurecheck   assurecheck/stub.cpp)
lodestar_add_module(lodestar_riskai        riskai/stub.cpp)
lodestar_add_module(lodestar_integratehub  integratehub/stub.cpp)
lodestar_add_module(lodestar_adapters      adapters/stub.cpp)
lodestar_add_module(lodestar_api           api/stub.cpp)

# --- Aggregate core library ------------------------------------------------
add_library(lodestar_core INTERFACE)
target_link_libraries(lodestar_core INTERFACE
    lodestar_common
    lodestar_persistence
    lodestar_tracelink
    lodestar_scenario
    lodestar_testforge
    lodestar_assurecheck
    lodestar_riskai
    lodestar_integratehub
    lodestar_adapters
    lodestar_api)

# --- Smoke path (self-verifying CLI) ----------------------------------------
add_executable(lodestar_smoke smoke/main.cpp)
target_link_libraries(lodestar_smoke PRIVATE
    lodestar_common
    lodestar_persistence
    lodestar_tracelink)
target_compile_definitions(lodestar_smoke PRIVATE
    LODESTAR_MIGRATIONS_DIR="${CMAKE_CURRENT_SOURCE_DIR}/persistence/migrations")
