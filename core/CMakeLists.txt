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
    tracelink/TraceGraph.cpp
    tracelink/GraphEngine.cpp
    tracelink/Types.cpp
    tracelink/StateMachine.cpp
    tracelink/TraceLinkService.cpp
    tracelink/BaselineService.cpp
    tracelink/ChangeRequestService.cpp
    tracelink/RulesEngine.cpp
    tracelink/IoService.cpp
    tracelink/ViewModelFactory.cpp)
target_link_libraries(lodestar_tracelink PUBLIC lodestar_common lodestar_persistence)

# --- ScenarioForge (real GNSS math, Phase 4) ---------------------------------
lodestar_add_module(lodestar_scenario
    scenario/stub.cpp
    scenario/frames/TimeSystem.cpp
    scenario/frames/Frames.cpp
    scenario/frames/Geometry.cpp
    scenario/orbit/Keplerian.cpp
    scenario/orbit/Tle.cpp
    scenario/orbit/Sgp4.cpp
    scenario/orbit/Constellation.cpp
    scenario/rinex/RinexNav.cpp
    scenario/rinex/RinexObs.cpp
    scenario/nmea/NmeaSentence.cpp
    scenario/nmea/NmeaGenerator.cpp
    scenario/pvt/Pseudorange.cpp
    scenario/pvt/Doppler.cpp
    scenario/pvt/Solver.cpp
    scenario/errors/ClockModel.cpp
    scenario/errors/Ionosphere.cpp
    scenario/errors/Troposphere.cpp
    scenario/errors/ErrorModelConfig.cpp
    scenario/sbas/SbasMessage.cpp
    scenario/sbas/SbasCorrections.cpp
    scenario/sbas/Integrity.cpp
    scenario/Scenario.cpp)
target_link_libraries(lodestar_scenario PUBLIC lodestar_common)

# --- Stub modules (empty libraries that compile) ----------------------------
lodestar_add_module(lodestar_testforge
    testforge/TestForgeDao.cpp
    testforge/PlanGenerator.cpp
    testforge/TestRunner.cpp
    testforge/ReportGenerator.cpp)
target_link_libraries(lodestar_testforge PUBLIC lodestar_common lodestar_persistence)
target_compile_definitions(lodestar_testforge PRIVATE
    LODESTAR_TESTFORGE_MIGRATIONS_DIR="${CMAKE_CURRENT_SOURCE_DIR}/persistence/migrations")
lodestar_add_module(lodestar_assurecheck   assurecheck/stub.cpp)
lodestar_add_module(lodestar_riskai        riskai/stub.cpp)
lodestar_add_module(lodestar_integratehub  integratehub/stub.cpp)

# --- Adapters (Phase 5): external tool / LLM / Python integration -----------
lodestar_add_module(lodestar_adapters
    adapters/Json.cpp
    adapters/AdapterRegistry.cpp
    adapters/MockAdapter.cpp
    adapters/LlmAdapter.cpp
    adapters/PythonAdapter.cpp
    adapters/SpirentAdapter.cpp
    adapters/RsAdapter.cpp
    adapters/SkydelAdapter.cpp
    adapters/HttpClient.cpp)
target_link_libraries(lodestar_adapters PUBLIC lodestar_common)
if(WIN32)
    target_link_libraries(lodestar_adapters PUBLIC ws2_32)
endif()

# --- Thin C++ REST API (Phase 5): embedded HTTP server + endpoints ----------
lodestar_add_module(lodestar_api
    api/HttpServer.cpp
    api/ApiServer.cpp
    api/ApiKeyService.cpp
    api/TraceLinkApiServer.cpp)
target_link_libraries(lodestar_api PUBLIC
    lodestar_common
    lodestar_adapters
    lodestar_scenario
    lodestar_tracelink)
if(WIN32)
    target_link_libraries(lodestar_api PUBLIC ws2_32)
endif()

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
add_executable(lodestar_smoke
    smoke/main.cpp
    smoke/scenario_smoke.cpp
    smoke/adapters_api_smoke.cpp
    smoke/testforge_smoke.cpp
    smoke/tracelink_smoke.cpp)
target_link_libraries(lodestar_smoke PRIVATE
    lodestar_common
    lodestar_persistence
    lodestar_tracelink
    lodestar_scenario
    lodestar_adapters
    lodestar_api
    lodestar_testforge)
if(WIN32)
    target_link_libraries(lodestar_smoke PRIVATE ws2_32)
endif()
target_compile_definitions(lodestar_smoke PRIVATE
    LODESTAR_MIGRATIONS_DIR="${CMAKE_CURRENT_SOURCE_DIR}/persistence/migrations"
    LODESTAR_TESTFORGE_MIGRATIONS_DIR="${CMAKE_CURRENT_SOURCE_DIR}/persistence/migrations")

# --- WP-1 unit/integration tests (test-first, written by scrum-master) -------
# Requires TraceLinkService + enriched models + migrations 003/004. Build with
# -DLODESTAR_BUILD_TESTS=ON once the WP-1 engineer implements the contract.
if(LODESTAR_BUILD_TESTS)
    add_executable(lodestar_wp1_tests
        test/wp1_tracelink_tests.cpp)
    target_link_libraries(lodestar_wp1_tests PRIVATE
        lodestar_common
        lodestar_persistence
        lodestar_tracelink)
    target_compile_definitions(lodestar_wp1_tests PRIVATE
        LODESTAR_MIGRATIONS_DIR="${CMAKE_CURRENT_SOURCE_DIR}/persistence/migrations")

    add_executable(lodestar_wp2_tests
        test/wp2_graph_engine_tests.cpp)
    target_link_libraries(lodestar_wp2_tests PRIVATE
        lodestar_common
        lodestar_persistence
        lodestar_tracelink)
    target_compile_definitions(lodestar_wp2_tests PRIVATE
        LODESTAR_MIGRATIONS_DIR="${CMAKE_CURRENT_SOURCE_DIR}/persistence/migrations")

    add_executable(lodestar_wp3_tests
        test/wp3_rules_engine_tests.cpp)
    target_link_libraries(lodestar_wp3_tests PRIVATE
        lodestar_common
        lodestar_persistence
        lodestar_tracelink)
    target_compile_definitions(lodestar_wp3_tests PRIVATE
        LODESTAR_MIGRATIONS_DIR="${CMAKE_CURRENT_SOURCE_DIR}/persistence/migrations")

    add_executable(lodestar_wp4_tests
        test/wp4_audit_baseline_tests.cpp)
    target_link_libraries(lodestar_wp4_tests PRIVATE
        lodestar_common
        lodestar_persistence
        lodestar_tracelink)
    target_compile_definitions(lodestar_wp4_tests PRIVATE
        LODESTAR_MIGRATIONS_DIR="${CMAKE_CURRENT_SOURCE_DIR}/persistence/migrations")

    add_executable(lodestar_wp5_tests
        test/wp5_import_export_tests.cpp)
    target_link_libraries(lodestar_wp5_tests PRIVATE
        lodestar_common
        lodestar_persistence
        lodestar_tracelink)
    target_compile_definitions(lodestar_wp5_tests PRIVATE
        LODESTAR_MIGRATIONS_DIR="${CMAKE_CURRENT_SOURCE_DIR}/persistence/migrations")

    add_executable(lodestar_wp6_tests
        test/wp6_api_tests.cpp)
    target_link_libraries(lodestar_wp6_tests PRIVATE
        lodestar_common
        lodestar_persistence
        lodestar_tracelink
        lodestar_api
        lodestar_adapters)
    if(WIN32)
        target_link_libraries(lodestar_wp6_tests PRIVATE ws2_32)
    endif()
    target_compile_definitions(lodestar_wp6_tests PRIVATE
        LODESTAR_MIGRATIONS_DIR="${CMAKE_CURRENT_SOURCE_DIR}/persistence/migrations")

    add_executable(lodestar_wp7_tests
        test/wp7_view_models_tests.cpp)
    target_link_libraries(lodestar_wp7_tests PRIVATE
        lodestar_common
        lodestar_persistence
        lodestar_tracelink)
    target_compile_definitions(lodestar_wp7_tests PRIVATE
        LODESTAR_MIGRATIONS_DIR="${CMAKE_CURRENT_SOURCE_DIR}/persistence/migrations")

    add_executable(lodestar_wp8_tests
        test/wp8_hardening_tests.cpp)
    target_link_libraries(lodestar_wp8_tests PRIVATE
        lodestar_common
        lodestar_persistence
        lodestar_tracelink)
    target_compile_definitions(lodestar_wp8_tests PRIVATE
        LODESTAR_MIGRATIONS_DIR="${CMAKE_CURRENT_SOURCE_DIR}/persistence/migrations")

    # --- WP-A: search + pagination (FTS5 ranked search, limit/offset) ------
    add_executable(lodestar_wpA_tests
        test/wpA_tests.cpp)
    target_link_libraries(lodestar_wpA_tests PRIVATE
        lodestar_common
        lodestar_persistence
        lodestar_tracelink)
    target_compile_definitions(lodestar_wpA_tests PRIVATE
        LODESTAR_MIGRATIONS_DIR="${CMAKE_CURRENT_SOURCE_DIR}/persistence/migrations")

    # --- WP-C: hierarchy tree (parent/child, subtree, reorder) -------------
    add_executable(lodestar_wpC_tests
        test/wpC_tests.cpp)
    target_link_libraries(lodestar_wpC_tests PRIVATE
        lodestar_common
        lodestar_persistence
        lodestar_tracelink)
    target_compile_definitions(lodestar_wpC_tests PRIVATE
        LODESTAR_MIGRATIONS_DIR="${CMAKE_CURRENT_SOURCE_DIR}/persistence/migrations")

    # --- WP-F: robustness hardening (error codes, validation, backup, ...) -
    add_executable(lodestar_wpF_tests
        test/wpF_tests.cpp)
    target_link_libraries(lodestar_wpF_tests PRIVATE
        lodestar_common
        lodestar_persistence
        lodestar_tracelink)
    target_compile_definitions(lodestar_wpF_tests PRIVATE
        LODESTAR_MIGRATIONS_DIR="${CMAKE_CURRENT_SOURCE_DIR}/persistence/migrations")

    # --- WP-B: change management (restore/rollback + change-request workflow) -
    add_executable(lodestar_wpB_tests
        test/wpB_tests.cpp)
    target_link_libraries(lodestar_wpB_tests PRIVATE
        lodestar_common
        lodestar_persistence
        lodestar_tracelink)
    target_compile_definitions(lodestar_wpB_tests PRIVATE
        LODESTAR_MIGRATIONS_DIR="${CMAKE_CURRENT_SOURCE_DIR}/persistence/migrations")

    # --- WP-D: coverage + evidence (by method + DO-178C evidence package) ---
    add_executable(lodestar_wpD_tests
        test/wpD_tests.cpp)
    target_link_libraries(lodestar_wpD_tests PRIVATE
        lodestar_common
        lodestar_persistence
        lodestar_tracelink)
    target_compile_definitions(lodestar_wpD_tests PRIVATE
        LODESTAR_MIGRATIONS_DIR="${CMAKE_CURRENT_SOURCE_DIR}/persistence/migrations")

    # --- WP-E: API auth/keys + duplicate detection --------------------------
    add_executable(lodestar_wpE_tests
        test/wpE_tests.cpp)
    target_link_libraries(lodestar_wpE_tests PRIVATE
        lodestar_common
        lodestar_persistence
        lodestar_tracelink
        lodestar_api
        lodestar_adapters)
    if(WIN32)
        target_link_libraries(lodestar_wpE_tests PRIVATE ws2_32)
    endif()
    target_compile_definitions(lodestar_wpE_tests PRIVATE
        LODESTAR_MIGRATIONS_DIR="${CMAKE_CURRENT_SOURCE_DIR}/persistence/migrations")
endif()
