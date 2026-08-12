# ui/CMakeLists.txt
# Qt Widgets desktop UI for the four WP-7 views. Built only when
# LODESTAR_BUILD_UI=ON (top-level CMakeLists only adds this subdirectory then).
# When Qt is unavailable we fall back to the plain stub library so the loop
# still configures cleanly.

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 QUIET COMPONENTS Widgets)
if(Qt6_FOUND)
    set(QT_PKG Qt6)
else()
    find_package(Qt5 QUIET COMPONENTS Widgets)
    set(QT_PKG Qt5)
endif()

if(QT_PKG)
    add_library(lodestar_ui STATIC
        MatrixView.cpp
        GraphView.cpp
        ImpactView.cpp
        CoverageDashboardView.cpp
        ProjectTreeView.cpp
        DetailPanelView.cpp
        BaselineDiffView.cpp
        DocumentView.cpp
        MainWindow.cpp)
    target_link_libraries(lodestar_ui PUBLIC
        ${QT_PKG}::Widgets
        lodestar_core)
    target_include_directories(lodestar_ui PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}/..")
    target_compile_features(lodestar_ui PUBLIC cxx_std_17)
else()
    # No Qt installed: keep the plain placeholder library (never built in this
    # loop because LODESTAR_BUILD_UI defaults OFF).
    add_library(lodestar_ui STATIC stub.cpp)
    target_compile_features(lodestar_ui PUBLIC cxx_std_17)
endif()
