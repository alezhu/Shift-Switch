set(SHIFTSWITCHUI_SOURCES
    ShiftSwitchUI.cpp
    ShiftSwitchUI.h
    CApp.cpp
    CApp.h
    CLog.cpp
    CLog.h
    CProcessBase.cpp
    CProcessBase.h
    CProcessChild.cpp
    CProcessChild.h
    CProcessMain.cpp
    CProcessMain.h
    CrashReporter.cpp
    CrashReporter.h
    HookController.cpp
    HookController.h
    AppSettings.h
    framework.h
    IconHelper.h
    IWindow.h
    LogOutput.h
    pch.cpp
    pch.h
    resource.h
    StateCoordinator.h
    targetver.h
    ShiftSwitchUI.rc
    ShiftSwitchUI.manifest
)

add_executable(ShiftSwitchUI WIN32 ${SHIFTSWITCHUI_SOURCES})

target_compile_definitions(ShiftSwitchUI PRIVATE
    _WINDOWS
)

set_source_files_properties(
    CProcessBase.cpp
    CProcessMain.cpp
    PROPERTIES COMPILE_DEFINITIONS "VERSION_STRING=\"${PROJECT_VERSION}\""
)

set_target_properties(ShiftSwitchUI PROPERTIES
    OUTPUT_NAME "${UI_EXE_NAME}"
    WIN32_EXECUTABLE TRUE
)

target_include_directories(ShiftSwitchUI PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}"
)

target_precompile_headers(ShiftSwitchUI PRIVATE pch.h)

target_link_libraries(ShiftSwitchUI PRIVATE
    ShiftSwitchCommon
    kernel32
    user32
    gdi32
    shlwapi
    comctl32
    dbghelp
    version
)
