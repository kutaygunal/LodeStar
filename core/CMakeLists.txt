# core/CMakeLists.txt
# C++ service core. Each module is a static library target.

include(LodestarUtils)

# --- Stub modules (empty libraries that compile) ---------------------------
lodestar_add_module(lodestar_common        common/stub.cpp)
lodestar_add_module(lodestar_persistence   persistence/stub.cpp)
lodestar_add_module(lodestar_tracelink     tracelink/stub.cpp)
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
