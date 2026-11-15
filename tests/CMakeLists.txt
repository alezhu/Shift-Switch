set(TESTS_SOURCES
    TestMain.cpp
    ShiftTapTests.cpp
    ShiftTapTests.h
    ExclusionsIntegrationTests.cpp
    ExclusionsIntegrationTests.h
    HookLifecycleIntegrationTests.cpp
    HookLifecycleIntegrationTests.h
    IpcIntegrationTests.cpp
    IpcIntegrationTests.h
    TestHarness.h
    IconHelperTests.h
    AppSettingsTests.h
    "${CMAKE_SOURCE_DIR}/ShiftSwitchUI/HookController.cpp"
)

add_executable(ShiftSwitchTests ${TESTS_SOURCES})

target_compile_definitions(ShiftSwitchTests PRIVATE
    _CONSOLE
)

target_include_directories(ShiftSwitchTests PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}"
    "${CMAKE_SOURCE_DIR}/ShiftSwitchUI"
    "${CMAKE_SOURCE_DIR}/ShiftSwitch"
)

# Output directory: bin/tests/<ARCH>/<CONFIG>/
if(CMAKE_CONFIGURATION_TYPES)
    foreach(OUTPUTCONFIG ${CMAKE_CONFIGURATION_TYPES})
        string(TOUPPER ${OUTPUTCONFIG} OUTPUTCONFIG_UPPER)
        set_target_properties(ShiftSwitchTests PROPERTIES
            RUNTIME_OUTPUT_DIRECTORY_${OUTPUTCONFIG_UPPER} "${CMAKE_SOURCE_DIR}/bin/tests/${ARCH_DIR}/${OUTPUTCONFIG}"
        )
    endforeach()
elseif(CMAKE_BUILD_TYPE)
    set_target_properties(ShiftSwitchTests PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin/tests/${ARCH_DIR}/${CMAKE_BUILD_TYPE}"
    )
endif()

target_link_libraries(ShiftSwitchTests PRIVATE
    ShiftSwitchCommon
    kernel32
    user32
    gdi32
    shlwapi
    comctl32
)

# Ensure DLL and UI are built before tests
add_dependencies(ShiftSwitchTests ShiftSwitch ShiftSwitchUI)

# Register CTest test
add_test(NAME ShiftSwitchTests COMMAND ShiftSwitchTests)
