cmake_minimum_required(VERSION 3.25)

# VERSION is the single source of the release version (MAJOR.MINOR.PATCH).
file(STRINGS "${CMAKE_CURRENT_SOURCE_DIR}/VERSION" STAT_WISP_VERSION LIMIT_COUNT 1)
string(STRIP "${STAT_WISP_VERSION}" STAT_WISP_VERSION)
if(NOT STAT_WISP_VERSION MATCHES "^[0-9]+\\.[0-9]+\\.[0-9]+$")
    message(FATAL_ERROR "VERSION must contain MAJOR.MINOR.PATCH, found '${STAT_WISP_VERSION}'.")
endif()
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/VERSION")

project(stat-wisp VERSION ${STAT_WISP_VERSION} LANGUAGES CXX RC)

set(STAT_WISP_GENERATED_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated")
configure_file(src/version.h.in "${STAT_WISP_GENERATED_DIR}/version.h" @ONLY)
configure_file(src/app.manifest.in "${STAT_WISP_GENERATED_DIR}/app.manifest" @ONLY)

option(STAT_WISP_BUILD_TESTS "Build stat-wisp logic tests" ON)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
if(MSVC)
    add_compile_options($<$<CONFIG:Release>:/O2>)
    add_compile_definitions($<$<CONFIG:Release>:NDEBUG>)
    add_link_options($<$<CONFIG:Release>:/INCREMENTAL:NO>)
endif()

add_library(stat_wisp_core STATIC
    src/core/Log.cpp
    src/core/MetricFormatter.cpp
    src/core/SemanticVersion.cpp
    src/settings/Settings.cpp
    src/update/Sha256.cpp
    src/update/UpdateManifest.cpp
)
target_include_directories(stat_wisp_core PUBLIC src "${STAT_WISP_GENERATED_DIR}")
target_compile_definitions(stat_wisp_core PUBLIC UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX WINVER=0x0A00 _WIN32_WINNT=0x0A00)
target_link_libraries(stat_wisp_core PUBLIC bcrypt shell32 ole32)

add_executable(stat-wisp WIN32
    src/main.cpp
    src/app/Application.cpp
    src/app/CrashHandler.cpp
    src/app/MessageWindow.cpp
    src/monitoring/MonitorService.cpp
    src/providers/AmdProvider.cpp
    src/providers/BatteryProvider.cpp
    src/providers/CpuProvider.cpp
    src/providers/DiskProvider.cpp
    src/providers/GpuPerformanceProvider.cpp
    src/providers/GpuProvider.cpp
    src/providers/MemoryProvider.cpp
    src/providers/NetworkProvider.cpp
    src/providers/NvidiaProvider.cpp
    src/providers/SensorFallbackProvider.cpp
    src/settings/SettingsWindow.cpp
    src/startup/StartupManager.cpp
    src/tray/TrayManager.cpp
    src/tray/TrayRenderer.cpp
    src/update/UpdateService.cpp
    src/stat-wisp.rc
)
target_include_directories(stat-wisp PRIVATE src "${STAT_WISP_GENERATED_DIR}")
target_compile_definitions(stat-wisp PRIVATE UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX WINVER=0x0A00 _WIN32_WINNT=0x0A00)
target_link_libraries(stat-wisp PRIVATE
    stat_wisp_core
    advapi32
    comctl32
    comsuppw
    gdi32
    iphlpapi
    ole32
    oleaut32
    pdh
    powrprof
    shell32
    user32
    wbemuuid
    winhttp
)

if(MSVC)
    target_compile_options(stat_wisp_core PRIVATE /W4 /permissive- /EHsc /utf-8 /Zc:__cplusplus)
    target_compile_options(stat-wisp PRIVATE /W4 /permissive- /EHsc /utf-8 /Zc:__cplusplus)
    target_compile_options(stat_wisp_core PRIVATE /guard:cf)
    target_compile_options(stat-wisp PRIVATE /guard:cf)
    target_link_options(stat-wisp PRIVATE /OPT:REF /OPT:ICF /MANIFEST:NO /DYNAMICBASE /HIGHENTROPYVA /NXCOMPAT /guard:cf)
endif()

install(TARGETS stat-wisp RUNTIME DESTINATION .)

include(CTest)
if(BUILD_TESTING AND STAT_WISP_BUILD_TESTS)
    add_executable(stat-wisp-tests tests/CoreTests.cpp)
    target_link_libraries(stat-wisp-tests PRIVATE stat_wisp_core)
    target_compile_definitions(stat-wisp-tests PRIVATE UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX WINVER=0x0A00 _WIN32_WINNT=0x0A00)
    if(MSVC)
        target_compile_options(stat-wisp-tests PRIVATE /W4 /permissive- /EHsc /utf-8 /Zc:__cplusplus)
    endif()
    add_test(NAME stat-wisp-core COMMAND stat-wisp-tests)
    add_executable(stat-wisp-runtime-tests tests/RuntimeTests.cpp src/tray/TrayRenderer.cpp
        src/providers/GpuProvider.cpp src/providers/GpuPerformanceProvider.cpp
        src/providers/NvidiaProvider.cpp src/providers/AmdProvider.cpp)
    target_link_libraries(stat-wisp-runtime-tests PRIVATE stat_wisp_core user32 gdi32 advapi32 pdh)
    if(MSVC)
        target_compile_options(stat-wisp-runtime-tests PRIVATE /W4 /permissive- /EHsc /utf-8)
    endif()
    add_test(NAME stat-wisp-runtime COMMAND stat-wisp-runtime-tests)
    add_executable(stat-wisp-update-tests tests/UpdateTests.cpp src/update/UpdateService.cpp)
    target_include_directories(stat-wisp-update-tests PRIVATE src "${STAT_WISP_GENERATED_DIR}")
    target_link_libraries(stat-wisp-update-tests PRIVATE stat_wisp_core advapi32 shell32 user32 winhttp)
    if(MSVC)
        target_compile_options(stat-wisp-update-tests PRIVATE /W4 /permissive- /EHsc /utf-8 /Zc:__cplusplus)
    endif()
    add_test(NAME stat-wisp-update COMMAND stat-wisp-update-tests)
    # Same application with a deliberately stuck worker, never packaged.
    get_target_property(app_sources stat-wisp SOURCES)
    get_target_property(app_libraries stat-wisp LINK_LIBRARIES)
    add_executable(stat-wisp-shutdown-test WIN32 ${app_sources})
    target_include_directories(stat-wisp-shutdown-test PRIVATE src "${STAT_WISP_GENERATED_DIR}")
    target_link_libraries(stat-wisp-shutdown-test PRIVATE ${app_libraries})
    target_compile_definitions(stat-wisp-shutdown-test PRIVATE STAT_WISP_TEST_STALLED_PROVIDER)
    if(MSVC)
        target_compile_options(stat-wisp-shutdown-test PRIVATE /W4 /permissive- /EHsc /utf-8)
        target_link_options(stat-wisp-shutdown-test PRIVATE /MANIFEST:NO)
    endif()
    find_program(POWERSHELL_EXECUTABLE NAMES pwsh powershell REQUIRED)
    add_test(NAME stat-wisp-exit COMMAND ${POWERSHELL_EXECUTABLE} -NoProfile -File
        ${CMAKE_CURRENT_SOURCE_DIR}/scripts/test-exit.ps1 -Executable $<TARGET_FILE:stat-wisp>
        -TestDirectory ${CMAKE_CURRENT_BINARY_DIR}/exit-test)
    add_test(NAME stat-wisp-stalled-exit COMMAND ${POWERSHELL_EXECUTABLE} -NoProfile -File
        ${CMAKE_CURRENT_SOURCE_DIR}/scripts/test-exit.ps1 -Executable $<TARGET_FILE:stat-wisp-shutdown-test>
        -TestDirectory ${CMAKE_CURRENT_BINARY_DIR}/stalled-exit-test -Stalled)
    set_tests_properties(stat-wisp-exit stat-wisp-stalled-exit PROPERTIES RUN_SERIAL TRUE TIMEOUT 20)
endif()

if(BUILD_TESTING AND STAT_WISP_BUILD_TESTS)
    add_test(NAME stat-wisp-settings-dialog COMMAND ${POWERSHELL_EXECUTABLE} -NoProfile -File
        ${CMAKE_CURRENT_SOURCE_DIR}/scripts/test-settings-dialog.ps1 -Executable $<TARGET_FILE:stat-wisp>
        -TestDirectory ${CMAKE_CURRENT_BINARY_DIR}/settings-dialog-test)
    set_tests_properties(stat-wisp-settings-dialog PROPERTIES RUN_SERIAL TRUE TIMEOUT 20)
endif()
