cmake_minimum_required(VERSION 3.25)

project(gate-monitor VERSION 0.3.0 LANGUAGES CXX RC)

option(GATE_MONITOR_BUILD_TESTS "Build gate-monitor logic tests" ON)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

add_library(gate_monitor_core STATIC
    src/core/MetricFormatter.cpp
    src/core/SemanticVersion.cpp
    src/settings/Settings.cpp
)
target_include_directories(gate_monitor_core PUBLIC src)
target_compile_definitions(gate_monitor_core PUBLIC UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX WINVER=0x0A00 _WIN32_WINNT=0x0A00)
target_link_libraries(gate_monitor_core PUBLIC shell32 ole32)

add_executable(gate-monitor WIN32
    src/main.cpp
    src/app/Application.cpp
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
    src/gate-monitor.rc
)
target_include_directories(gate-monitor PRIVATE src)
target_compile_definitions(gate-monitor PRIVATE UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX WINVER=0x0A00 _WIN32_WINNT=0x0A00)
target_link_libraries(gate-monitor PRIVATE
    gate_monitor_core
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
)

if(MSVC)
    target_compile_options(gate_monitor_core PRIVATE /W4 /permissive- /EHsc /utf-8 /Zc:__cplusplus)
    target_compile_options(gate-monitor PRIVATE /W4 /permissive- /EHsc /utf-8 /Zc:__cplusplus)
    target_link_options(gate-monitor PRIVATE /OPT:REF /OPT:ICF /MANIFEST:NO)
endif()

install(TARGETS gate-monitor RUNTIME DESTINATION .)

include(CTest)
if(BUILD_TESTING AND GATE_MONITOR_BUILD_TESTS)
    add_executable(gate-monitor-tests tests/CoreTests.cpp)
    target_link_libraries(gate-monitor-tests PRIVATE gate_monitor_core)
    target_compile_definitions(gate-monitor-tests PRIVATE UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX WINVER=0x0A00 _WIN32_WINNT=0x0A00)
    if(MSVC)
        target_compile_options(gate-monitor-tests PRIVATE /W4 /permissive- /EHsc /utf-8 /Zc:__cplusplus)
    endif()
    add_test(NAME gate-monitor-core COMMAND gate-monitor-tests)
endif()
