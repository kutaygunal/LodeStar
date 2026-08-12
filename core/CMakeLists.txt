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
    api/ApiServer.cpp)
target_link_libraries(lodestar_api PUBLIC
    lodestar_common
    lodestar_adapters
    lodestar_scenario)
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
    smoke/testforge_smoke.cpp)
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
