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
    tracelink/ReviewService.cpp
    tracelink/RulesEngine.cpp
    tracelink/IoService.cpp
    tracelink/ViewModelFactory.cpp
    tracelink/UiWiringService.cpp
    tracelink/CoverageService.cpp
    tracelink/SuspectService.cpp
    tracelink/ComplianceService.cpp
    tracelink/RbacService.cpp)
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
lodestar_add_module(lodestar_assurecheck
    assurecheck/AssureCheckService.cpp
    assurecheck/ComplianceEngine.cpp
    assurecheck/EvidenceService.cpp
    assurecheck/PerformanceService.cpp
    assurecheck/ReportService.cpp
    assurecheck/DashboardService.cpp
    assurecheck/stub.cpp)
target_link_libraries(lodestar_assurecheck PUBLIC
    lodestar_common lodestar_persistence lodestar_tracelink lodestar_testforge)
# --- RiskAI: LLM-assisted FMEA (S1 Phase 3) ---------------------------------
lodestar_add_module(lodestar_riskai
    riskai/RiskAiService.cpp)
target_link_libraries(lodestar_riskai PUBLIC
    lodestar_common lodestar_adapters)
# --- IntegrateHub: cross-disciplinary issue/coordination model (S1 Phase 4) ---
lodestar_add_module(lodestar_integratehub
    integratehub/IntegrateHubService.cpp)
target_link_libraries(lodestar_integratehub PUBLIC
    lodestar_common lodestar_persistence)

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
    api/TraceLinkApiServer.cpp
    api/AssureCheckApiServer.cpp)
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

    # --- WP-G: Qt UI wiring (Qt-independent view-model/service wiring) ------
    add_executable(lodestar_wpG_tests
        test/wpG_tests.cpp)
    target_link_libraries(lodestar_wpG_tests PRIVATE
        lodestar_common
        lodestar_persistence
        lodestar_tracelink)
    target_compile_definitions(lodestar_wpG_tests PRIVATE
        LODESTAR_MIGRATIONS_DIR="${CMAKE_CURRENT_SOURCE_DIR}/persistence/migrations")

    # --- Phase 10 WP-1: suspect-link workflow (migration 013) ---------------
    add_executable(lodestar_wp1_suspect_tests
        test/wp1_suspect_tests.cpp)
    target_link_libraries(lodestar_wp1_suspect_tests PRIVATE
        lodestar_common
        lodestar_persistence
        lodestar_tracelink)
    target_compile_definitions(lodestar_wp1_suspect_tests PRIVATE
        LODESTAR_MIGRATIONS_DIR="${CMAKE_CURRENT_SOURCE_DIR}/persistence/migrations")

    # --- Phase 10 WP-2: review/comment/approval (migration 014) ------------
    add_executable(lodestar_wp2_review_tests
        test/wp2_review_tests.cpp)
    target_link_libraries(lodestar_wp2_review_tests PRIVATE
        lodestar_common
        lodestar_persistence
        lodestar_tracelink)
    target_compile_definitions(lodestar_wp2_review_tests PRIVATE
        LODESTAR_MIGRATIONS_DIR="${CMAKE_CURRENT_SOURCE_DIR}/persistence/migrations")

    # --- Phase 10 WP-3: compliance templates/checklists (migration 015) ----
    add_executable(lodestar_wp3_compliance_tests
        test/wp3_compliance_tests.cpp)
    target_link_libraries(lodestar_wp3_compliance_tests PRIVATE
        lodestar_common
        lodestar_persistence
        lodestar_tracelink)
    target_compile_definitions(lodestar_wp3_compliance_tests PRIVATE
        LODESTAR_MIGRATIONS_DIR="${CMAKE_CURRENT_SOURCE_DIR}/persistence/migrations")

    # --- Phase 10 WP-4: roles/permissions + concurrency (migration 016) ----
    add_executable(lodestar_wp4_rbac_tests
        test/wp4_rbac_tests.cpp)
    target_link_libraries(lodestar_wp4_rbac_tests PRIVATE
        lodestar_common
        lodestar_persistence
        lodestar_tracelink)
    target_compile_definitions(lodestar_wp4_rbac_tests PRIVATE
        LODESTAR_MIGRATIONS_DIR="${CMAKE_CURRENT_SOURCE_DIR}/persistence/migrations")

    # --- Phase 10 WP-5: TestForge coverage wiring (migration 017 optional) --
    add_executable(lodestar_wp5_coverage_tests
        test/wp5_coverage_tests.cpp)
    target_link_libraries(lodestar_wp5_coverage_tests PRIVATE
        lodestar_common
        lodestar_persistence
        lodestar_tracelink
        lodestar_testforge)
    target_compile_definitions(lodestar_wp5_coverage_tests PRIVATE
        LODESTAR_MIGRATIONS_DIR="${CMAKE_CURRENT_SOURCE_DIR}/persistence/migrations"
        LODESTAR_TESTFORGE_MIGRATIONS_DIR="${CMAKE_CURRENT_SOURCE_DIR}/persistence/migrations")

    # --- Phase 10 WP-6: Qt UI shell (left-nav tree + detail panel) --------
    # Qt-independent wiring test (tree/detail data the Qt views consume). The
    # Qt UI build itself is verified separately with LODESTAR_BUILD_UI=ON.
    add_executable(lodestar_wp6_ui_tests
        test/wp6_ui_tests.cpp)
    target_link_libraries(lodestar_wp6_ui_tests PRIVATE
        lodestar_common
        lodestar_persistence
        lodestar_tracelink)
    target_compile_definitions(lodestar_wp6_ui_tests PRIVATE
        LODESTAR_MIGRATIONS_DIR="${CMAKE_CURRENT_SOURCE_DIR}/persistence/migrations")

    # --- Phase 10 WP-7: coverage dashboard + charts (needs WP-5 + WP-6) ---
    add_executable(lodestar_wp7_dashboard_tests
        test/wp7_dashboard_tests.cpp)
    target_link_libraries(lodestar_wp7_dashboard_tests PRIVATE
        lodestar_common
        lodestar_persistence
        lodestar_tracelink
        lodestar_testforge)
    target_compile_definitions(lodestar_wp7_dashboard_tests PRIVATE
        LODESTAR_MIGRATIONS_DIR="${CMAKE_CURRENT_SOURCE_DIR}/persistence/migrations"
        LODESTAR_TESTFORGE_MIGRATIONS_DIR="${CMAKE_CURRENT_SOURCE_DIR}/persistence/migrations")

    # --- Phase 10 WP-8: interactive traceability matrix -------------------
    add_executable(lodestar_wp8_matrix_tests
        test/wp8_matrix_tests.cpp)
    target_link_libraries(lodestar_wp8_matrix_tests PRIVATE
        lodestar_common
        lodestar_persistence
        lodestar_tracelink)
    target_compile_definitions(lodestar_wp8_matrix_tests PRIVATE
        LODESTAR_MIGRATIONS_DIR="${CMAKE_CURRENT_SOURCE_DIR}/persistence/migrations")

    # --- Phase 10 WP-9: baseline visual diff + rollback -------------------
    add_executable(lodestar_wp9_diff_tests
        test/wp9_diff_tests.cpp)
    target_link_libraries(lodestar_wp9_diff_tests PRIVATE
        lodestar_common
        lodestar_persistence
        lodestar_tracelink)
    target_compile_definitions(lodestar_wp9_diff_tests PRIVATE
        LODESTAR_MIGRATIONS_DIR="${CMAKE_CURRENT_SOURCE_DIR}/persistence/migrations")

    # --- Phase 10 WP-10: document-style authoring -------------------------
    add_executable(lodestar_wp10_doc_tests
        test/wp10_doc_tests.cpp)
    target_link_libraries(lodestar_wp10_doc_tests PRIVATE
        lodestar_common
        lodestar_persistence
        lodestar_tracelink)
    target_compile_definitions(lodestar_wp10_doc_tests PRIVATE
        LODESTAR_MIGRATIONS_DIR="${CMAKE_CURRENT_SOURCE_DIR}/persistence/migrations")

    # --- Phase 11 WP-1: AssureCheck standards + checklist data model (migration 019) ---
    # Test contract: docs/wp1-assurecheck-task.md (written by scrum-master).
    # Named lodestar_wp1_assurecheck_tests to avoid clobbering the existing
    # Phase-1 tracelink lodestar_wp1_tests regression target.
    add_executable(lodestar_wp1_assurecheck_tests
        test/wp1_assurecheck_tests.cpp)
    target_link_libraries(lodestar_wp1_assurecheck_tests PRIVATE
        lodestar_common
        lodestar_persistence
        lodestar_assurecheck)
    target_compile_definitions(lodestar_wp1_assurecheck_tests PRIVATE
        LODESTAR_MIGRATIONS_DIR="${CMAKE_CURRENT_SOURCE_DIR}/persistence/migrations")

    # --- Phase 11 WP-2: AssureCheck compliance engine (migration 020) ------
    # Test contract: docs/wp2-assurecheck-task.md (written by scrum-master).
    # Named lodestar_wp2_assurecheck_tests to avoid clobbering the existing
    # Phase-1 tracelink lodestar_wp2_tests regression target.
    add_executable(lodestar_wp2_assurecheck_tests
        test/wp2_assurecheck_tests.cpp)
    target_link_libraries(lodestar_wp2_assurecheck_tests PRIVATE
        lodestar_common
        lodestar_persistence
        lodestar_assurecheck)
    target_compile_definitions(lodestar_wp2_assurecheck_tests PRIVATE
        LODESTAR_MIGRATIONS_DIR="${CMAKE_CURRENT_SOURCE_DIR}/persistence/migrations")

    # --- Phase 11 WP-3: AssureCheck evidence + integration (TraceLink/TestForge) ---
    # Test contract: docs/wp3-assurecheck-task.md (written by scrum-master).
    # Named lodestar_wp3_assurecheck_tests to avoid clobbering the existing
    # Phase-1 tracelink lodestar_wp3_tests regression target.
    add_executable(lodestar_wp3_assurecheck_tests
        test/wp3_assurecheck_tests.cpp)
    target_link_libraries(lodestar_wp3_assurecheck_tests PRIVATE
        lodestar_common
        lodestar_persistence
        lodestar_assurecheck
        lodestar_tracelink
        lodestar_testforge)
    target_compile_definitions(lodestar_wp3_assurecheck_tests PRIVATE
        LODESTAR_MIGRATIONS_DIR="${CMAKE_CURRENT_SOURCE_DIR}/persistence/migrations")

    # --- Phase 11 WP-4: AssureCheck compliance reporting (HTML/CSV/JSON) ---
    # Test contract: docs/wp4-assurecheck-task.md (written by scrum-master).
    # Named lodestar_wp4_assurecheck_tests to avoid clobbering the existing
    # Phase-1 tracelink lodestar_wp4_tests regression target.
    add_executable(lodestar_wp4_assurecheck_tests
        test/wp4_assurecheck_tests.cpp)
    target_link_libraries(lodestar_wp4_assurecheck_tests PRIVATE
        lodestar_common
        lodestar_persistence
        lodestar_assurecheck
        lodestar_adapters)
    target_compile_definitions(lodestar_wp4_assurecheck_tests PRIVATE
        LODESTAR_MIGRATIONS_DIR="${CMAKE_CURRENT_SOURCE_DIR}/persistence/migrations")

    # --- Phase 11 WP-5: AssureCheck performance + hardening (migration 021) ---
    # Test contract: docs/wp5-assurecheck-task.md (written by scrum-master).
    # Named lodestar_wp5_assurecheck_tests to avoid clobbering the existing
    # Phase-1 tracelink lodestar_wp5_tests regression target.
    add_executable(lodestar_wp5_assurecheck_tests
        test/wp5_assurecheck_tests.cpp)
    target_link_libraries(lodestar_wp5_assurecheck_tests PRIVATE
        lodestar_common
        lodestar_persistence
        lodestar_assurecheck)
    target_compile_definitions(lodestar_wp5_assurecheck_tests PRIVATE
        LODESTAR_MIGRATIONS_DIR="${CMAKE_CURRENT_SOURCE_DIR}/persistence/migrations")

    # --- Phase 11 WP-6: AssureCheck REST API + compliance dashboard --------
    # Test contract: docs/wp6-assurecheck-task.md (written by scrum-master).
    # Named lodestar_wp6_assurecheck_tests to avoid clobbering the existing
    # Phase-1 tracelink lodestar_wp6_tests regression target.
    add_executable(lodestar_wp6_assurecheck_tests
        test/wp6_assurecheck_tests.cpp)
    target_link_libraries(lodestar_wp6_assurecheck_tests PRIVATE
        lodestar_common
        lodestar_persistence
        lodestar_assurecheck
        lodestar_api
        lodestar_adapters)
    if(WIN32)
        target_link_libraries(lodestar_wp6_assurecheck_tests PRIVATE ws2_32)
    endif()
    target_compile_definitions(lodestar_wp6_assurecheck_tests PRIVATE
        LODESTAR_MIGRATIONS_DIR="${CMAKE_CURRENT_SOURCE_DIR}/persistence/migrations")

    # --- S1 Phase 1: desktop app wiring (Qt-independent data contract) ------
    # Test contract: docs/s1-phase1-test.md (written by scrum-master).
    # Verifies the exact data the Qt views consume (refreshAll / projectTree /
    # detail) without needing a Qt runtime.
    add_executable(lodestar_s1_phase1_tests
        test/s1_phase1_tests.cpp)
    target_link_libraries(lodestar_s1_phase1_tests PRIVATE
        lodestar_common
        lodestar_persistence
        lodestar_tracelink)
    target_compile_definitions(lodestar_s1_phase1_tests PRIVATE
        LODESTAR_MIGRATIONS_DIR="${CMAKE_CURRENT_SOURCE_DIR}/persistence/migrations")

    # --- S1 Phase 4: IntegrateHub first slice (issue/coordination model) ------
    # Test contract: docs/s1-phase4-task.md (written by scrum-master).
    add_executable(lodestar_s1_phase4_tests
        test/s1_phase4_tests.cpp)
    target_link_libraries(lodestar_s1_phase4_tests PRIVATE
        lodestar_common
        lodestar_persistence
        lodestar_integratehub)
    target_compile_definitions(lodestar_s1_phase4_tests PRIVATE
        LODESTAR_MIGRATIONS_DIR="${CMAKE_CURRENT_SOURCE_DIR}/persistence/migrations")

    # --- S1 Phase 2: functional adapters (real Skydel + LLM invoke) -----------
    # Test contract: docs/s1-phase2-test.md (written by scrum-master).
    # Makes LlmAdapter::invoke("complete") and SkydelAdapter::invoke() perform
    # REAL out-of-process HTTP calls (with a simulated Skydel mode for CI).
    add_executable(lodestar_s1_phase2_tests
        test/s1_phase2_tests.cpp)
    target_link_libraries(lodestar_s1_phase2_tests PRIVATE
        lodestar_common
        lodestar_adapters)
    if(WIN32)
        target_link_libraries(lodestar_s1_phase2_tests PRIVATE ws2_32)
    endif()

    # --- S1 Phase 3: RiskAI first slice (hazard -> LLM -> FMEA) ---------------
    # Test contract: docs/s1-phase3-test.md (written by scrum-master).
    add_executable(lodestar_s1_phase3_tests
        test/s1_phase3_tests.cpp)
    target_link_libraries(lodestar_s1_phase3_tests PRIVATE
        lodestar_common
        lodestar_adapters
        lodestar_riskai)
    if(WIN32)
        target_link_libraries(lodestar_s1_phase3_tests PRIVATE ws2_32)
    endif()

    # --- S1 Phase 5: real-time / determinism validation (benchmarks) ----------
    # Test contract: docs/s1-phase5-test.md (written by scrum-master).
    # Times a core TraceGraph query with a monotonic clock, records min/avg/max
    # microseconds to docs/reports/s1-phase5-benchmarks.md, and verifies
    # determinism (same input -> byte-identical output).
    add_executable(lodestar_s1_phase5_tests
        test/s1_phase5_tests.cpp)
    target_link_libraries(lodestar_s1_phase5_tests PRIVATE
        lodestar_common
        lodestar_persistence
        lodestar_tracelink)
    target_compile_definitions(lodestar_s1_phase5_tests PRIVATE
        LODESTAR_MIGRATIONS_DIR="${CMAKE_CURRENT_SOURCE_DIR}/persistence/migrations")
endif()
