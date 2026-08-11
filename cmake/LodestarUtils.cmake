# LodestarUtils.cmake
# Shared CMake helpers for the Lodestar monorepo.

# lodestar_add_module(<name> <sources...>)
# Creates a static library target for a core module with common warnings.
function(lodestar_add_module name)
    add_library(${name} STATIC ${ARGN})
    target_include_directories(${name} PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}/..")
    target_compile_features(${name} PUBLIC cxx_std_17)

    if(MSVC)
        target_compile_options(${name} PRIVATE /W4 /permissive-)
    else()
        target_compile_options(${name} PRIVATE -Wall -Wextra -Wpedantic)
    endif()
endfunction()
