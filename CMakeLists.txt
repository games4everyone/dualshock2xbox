cmake_minimum_required(VERSION 3.20)
project(dualshock2xbox CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# portable core: parsing + mapping, no dependencies, builds everywhere
add_library(ds2x_core STATIC
    src/hid_report.cpp
    src/mapping.cpp
    src/ds4w_shaping.cpp
    src/names.cpp
    src/gyro.cpp
    src/output_report.cpp
    src/config.cpp
    src/profiles.cpp
    src/log.cpp
    src/test_mode.cpp
)
if(WIN32)
    target_sources(ds2x_core PRIVATE src/win_dynload.cpp)
    # win_dynload.cpp is wrapped in #ifdef DS2X_WINDOWS, so the *library* needs the
    # define too ; with it set only on the exe, this file compiled to nothing and
    # win::load() / win::hid_init were undefined at link time
    target_compile_definitions(ds2x_core PRIVATE DS2X_WINDOWS=1)
endif()
target_include_directories(ds2x_core PUBLIC src)

# the actual translator binary
add_executable(dualshock2xbox
    src/main.cpp
    src/source.cpp
    src/sink.cpp
)
target_link_libraries(dualshock2xbox PRIVATE ds2x_core)

if(WIN32)
    # hidapi.dll and ViGEmClient.dll are loaded at runtime (win_dynload),
    # so a plain build needs nothing but the Windows SDK ; ws2_32 is for the
    # test-mode GUI sockets
    target_compile_definitions(dualshock2xbox PRIVATE DS2X_WINDOWS=1)
    target_link_libraries(dualshock2xbox PRIVATE ws2_32)
endif()

# dependency-free tests for the portable core
option(DS2X_BUILD_TESTS "build tests" ON)
if(DS2X_BUILD_TESTS)
    enable_testing()
    add_executable(ds2x_tests
        tests/test_runner.cpp
        tests/test_hid_report.cpp
        tests/test_mapping.cpp
        tests/test_output_report.cpp
        tests/test_config.cpp
        tests/test_test_mode.cpp
        tests/test_gyro.cpp
    )
    target_link_libraries(ds2x_tests PRIVATE ds2x_core)
    add_test(NAME ds2x_tests COMMAND ds2x_tests)
endif()
