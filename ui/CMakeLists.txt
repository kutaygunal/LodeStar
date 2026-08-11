# ui/CMakeLists.txt
# Qt Widgets desktop UI (stub in this loop). Built only when LODESTAR_BUILD_UI=ON.
# Qt is not required for the Phase 2 scaffold; this is a plain placeholder library.

add_library(lodestar_ui STATIC stub.cpp)
target_compile_features(lodestar_ui PUBLIC cxx_std_17)
