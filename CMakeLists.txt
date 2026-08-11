cmake_minimum_required(VERSION 3.20)

project(Lodestar
    VERSION 0.1.0
    DESCRIPTION "GNSS/SBAS Integrated Test & Verification Platform"
    LANGUAGES CXX)

# ---------------------------------------------------------------------------
# Global options
# ---------------------------------------------------------------------------
option(LODESTAR_BUILD_TESTS "Build Lodestar tests" OFF)
option(LODESTAR_BUILD_UI    "Build the Qt Widgets UI (stub)" OFF)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

# ---------------------------------------------------------------------------
# Subprojects
# ---------------------------------------------------------------------------
add_subdirectory(core)

if(LODESTAR_BUILD_UI)
    add_subdirectory(ui)
endif()

# Python layer is a stub in this loop; nothing to compile yet.
