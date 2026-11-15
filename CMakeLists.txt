cmake_minimum_required(VERSION 3.20)

# Extract version from VERSION file (single source of truth)
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/VERSION")
    file(READ "${CMAKE_CURRENT_SOURCE_DIR}/VERSION" RAW_VERSION)
    string(STRIP "${RAW_VERSION}" PARSED_VERSION)
else()
    message(FATAL_ERROR "VERSION file not found in ${CMAKE_CURRENT_SOURCE_DIR}")
endif()

project(ShiftSwitch
    VERSION ${PARSED_VERSION}
    LANGUAGES C CXX RC
)

# Set C++ standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Architecture and target naming
if(CMAKE_SIZEOF_VOID_P EQUAL 8)
    set(ARCH_DIR "x64")
    set(HOOK_DLL_NAME "ShiftSwitch64")
    set(UI_EXE_NAME "ShiftSwitchUI64")
else()
    set(ARCH_DIR "x86")
    set(HOOK_DLL_NAME "ShiftSwitch")
    set(UI_EXE_NAME "ShiftSwitchUI")
endif()

# Global definitions
add_compile_definitions(
    UNICODE
    _UNICODE
    VERSION_MAJOR=${PROJECT_VERSION_MAJOR}
    VERSION_MINOR=${PROJECT_VERSION_MINOR}
    VERSION_PATCH=${PROJECT_VERSION_PATCH}
)

# Default output directory structure matching MSBuild:
# Executables and DLLs go into: bin/<CONFIG>/
# Static libraries go into: lib/<ARCH>/<CONFIG>/
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/bin")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/bin")

# Output directory structure
if(CMAKE_CONFIGURATION_TYPES)
    foreach(OUTPUTCONFIG ${CMAKE_CONFIGURATION_TYPES})
        string(TOUPPER ${OUTPUTCONFIG} OUTPUTCONFIG_UPPER)
        set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_${OUTPUTCONFIG_UPPER} "${CMAKE_CURRENT_SOURCE_DIR}/bin/${OUTPUTCONFIG}")
        set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_${OUTPUTCONFIG_UPPER} "${CMAKE_CURRENT_SOURCE_DIR}/bin/${OUTPUTCONFIG}")
        set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_${OUTPUTCONFIG_UPPER} "${CMAKE_CURRENT_SOURCE_DIR}/lib/${ARCH_DIR}/${OUTPUTCONFIG}")
    endforeach()
elseif(CMAKE_BUILD_TYPE)
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/bin/${CMAKE_BUILD_TYPE}")
    set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/bin/${CMAKE_BUILD_TYPE}")
    set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/lib/${ARCH_DIR}/${CMAKE_BUILD_TYPE}")
endif()

# Compiler options for MSVC
if(MSVC)
    add_compile_options(/permissive- /utf-8 /MP)
endif()

# Testing option
include(CTest)
option(BUILD_TESTING "Build the test suite" ON)

# Subdirectories
add_subdirectory(ShiftSwitchCommon)
add_subdirectory(ShiftSwitch)
add_subdirectory(ShiftSwitchUI)

if(BUILD_TESTING)
    add_subdirectory(tests)
endif()
