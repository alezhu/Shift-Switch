set(SHIFTSWITCH_SOURCES
    dllmain.cpp
    AsyncLogger.cpp
    AsyncLogger.h
    HookManager.cpp
    HookManager.h
    KeyProcessor.cpp
    KeyProcessor.h
    LayoutSwitcher.cpp
    LayoutSwitcher.h
    ProcessExclusionCache.cpp
    ProcessExclusionCache.h
    TsfSync.cpp
    TsfSync.h
    framework.h
    pch.cpp
    pch.h
    resource.h
    ShiftSwitch.rc
    ShiftSwitch.manifest
    Source.def
)

add_library(ShiftSwitch SHARED ${SHIFTSWITCH_SOURCES})

target_compile_definitions(ShiftSwitch PRIVATE
    SHIFTSWITCH_EXPORTS
    _WINDOWS
    _USRDLL
)

set_target_properties(ShiftSwitch PROPERTIES
    OUTPUT_NAME "${HOOK_DLL_NAME}"
)

# Output import library into bin/<CONFIG>/ alongside the DLL
if(CMAKE_CONFIGURATION_TYPES)
    foreach(OUTPUTCONFIG ${CMAKE_CONFIGURATION_TYPES})
        string(TOUPPER ${OUTPUTCONFIG} OUTPUTCONFIG_UPPER)
        set_target_properties(ShiftSwitch PROPERTIES
            ARCHIVE_OUTPUT_DIRECTORY_${OUTPUTCONFIG_UPPER} "${CMAKE_SOURCE_DIR}/bin/${OUTPUTCONFIG}"
        )
    endforeach()
elseif(CMAKE_BUILD_TYPE)
    set_target_properties(ShiftSwitch PROPERTIES
        ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin/${CMAKE_BUILD_TYPE}"
    )
endif()

target_include_directories(ShiftSwitch PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}"
)

target_precompile_headers(ShiftSwitch PRIVATE pch.h)

target_link_libraries(ShiftSwitch PRIVATE
    ShiftSwitchCommon
    kernel32
    user32
    shlwapi
    ole32
)
