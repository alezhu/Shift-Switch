add_library(ShiftSwitchCommon STATIC
    CHandle.cpp
    CHandle.h
    CMailslot.cpp
    CMailslot.h
    CMutex.cpp
    CMutex.h
    Exclusions.cpp
    Exclusions.h
    IpcMessages.h
    ProcessUtils.cpp
    ProcessUtils.h
    ScopedResource.h
    ScopeExit.h
    ShiftTap.h
    pch.cpp
    pch.h
)

target_include_directories(ShiftSwitchCommon PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}"
)

# Output archive directory: lib/<ARCH>/<CONFIG>/
if(CMAKE_CONFIGURATION_TYPES)
    foreach(OUTPUTCONFIG ${CMAKE_CONFIGURATION_TYPES})
        string(TOUPPER ${OUTPUTCONFIG} OUTPUTCONFIG_UPPER)
        set_target_properties(ShiftSwitchCommon PROPERTIES
            ARCHIVE_OUTPUT_DIRECTORY_${OUTPUTCONFIG_UPPER} "${CMAKE_SOURCE_DIR}/lib/${ARCH_DIR}/${OUTPUTCONFIG}"
        )
    endforeach()
elseif(CMAKE_BUILD_TYPE)
    set_target_properties(ShiftSwitchCommon PROPERTIES
        ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/lib/${ARCH_DIR}/${CMAKE_BUILD_TYPE}"
    )
endif()

target_precompile_headers(ShiftSwitchCommon PRIVATE pch.h)

target_link_libraries(ShiftSwitchCommon PUBLIC
    kernel32
    user32
)
