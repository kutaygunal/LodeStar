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

    # S1 Phase 1: runnable desktop app. Opens a DB, runs migrations, seeds a
    # small TraceLink graph, constructs MainWindow, calls refreshAll() + show().
    add_executable(lodestar_app app/main.cpp)
    target_link_libraries(lodestar_app PRIVATE lodestar_ui ${QT_PKG}::Widgets)
    target_include_directories(lodestar_app PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/..")
    target_compile_features(lodestar_app PRIVATE cxx_std_17)

    # Deploy the Qt runtime DLLs + platform plugins next to the app so the
    # contract launch command (./build/ui/Release/lodestar_app.exe --platform
    # offscreen) works without the Qt bin dir on PATH.
    if(QT_PKG STREQUAL "Qt6")
        get_filename_component(_QT_BIN "${Qt6_DIR}/../../../bin" ABSOLUTE)
        get_filename_component(_QT_PLUGINS "${Qt6_DIR}/../../../plugins" ABSOLUTE)
    else()
        get_filename_component(_QT_BIN "${Qt5_DIR}/../../../bin" ABSOLUTE)
        get_filename_component(_QT_PLUGINS "${Qt5_DIR}/../../../plugins" ABSOLUTE)
    endif()
    add_custom_command(TARGET lodestar_app POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${_QT_BIN}/${QT_PKG}Core.dll"
            "${_QT_BIN}/${QT_PKG}Gui.dll"
            "${_QT_BIN}/${QT_PKG}Widgets.dll"
            $<TARGET_FILE_DIR:lodestar_app>
        COMMAND ${CMAKE_COMMAND} -E make_directory
            $<TARGET_FILE_DIR:lodestar_app>/platforms
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${_QT_PLUGINS}/platforms/qoffscreen.dll"
            "${_QT_PLUGINS}/platforms/qwindows.dll"
            $<TARGET_FILE_DIR:lodestar_app>/platforms
        COMMENT "Deploying Qt runtime DLLs + platform plugins"
    )
else()
    # No Qt installed: keep the plain placeholder library (never built in this
    # loop because LODESTAR_BUILD_UI defaults OFF).
    add_library(lodestar_ui STATIC stub.cpp)
    target_compile_features(lodestar_ui PUBLIC cxx_std_17)
endif()
