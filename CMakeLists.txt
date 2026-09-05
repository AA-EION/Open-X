cmake_minimum_required(VERSION 3.24)
project(OpenXSuite VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(OPENX_BUILD_VST3 "Build VST3 targets" ON)
option(OPENX_BUILD_AU   "Build AudioUnit targets (macOS only)" ON)
option(OPENX_BUILD_AAX  "Build AAX targets" OFF)
option(OPENX_BUILD_UNINSTALLER "Build standalone GUI uninstaller application" ON)
option(OPENX_ENABLE_TESTS "Build DSP verification unit tests" ON)

include(FetchContent)
FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        8.0.4
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(JUCE)

include(cmake/OpenXHelpers.cmake)

set(OPENX_PLUGIN_FORMATS)
if(OPENX_BUILD_VST3)
    list(APPEND OPENX_PLUGIN_FORMATS VST3)
endif()
if(OPENX_BUILD_AU AND APPLE)
    list(APPEND OPENX_PLUGIN_FORMATS AU)
endif()
if(OPENX_BUILD_AAX)
    list(APPEND OPENX_PLUGIN_FORMATS AAX)
endif()

add_subdirectory(modules/openx_dsp)
add_subdirectory(modules/openx_ui)

add_subdirectory(plugins/EQ-X)
add_subdirectory(plugins/Comp-X)
add_subdirectory(plugins/Limit-X)
add_subdirectory(plugins/Verb-X)
add_subdirectory(plugins/MB-X)
add_subdirectory(plugins/DS-X)
add_subdirectory(plugins/Gate-X)

if(OPENX_BUILD_UNINSTALLER)
    add_subdirectory(uninstaller)
endif()

if(OPENX_ENABLE_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
