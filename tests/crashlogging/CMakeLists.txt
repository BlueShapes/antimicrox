cmake_minimum_required(VERSION 3.16)

project(antimicrox_crash_logging_tests LANGUAGES CXX)

find_package(Qt${QT_VERSION_MAJOR} COMPONENTS Core REQUIRED)

add_executable(CrashLoggingTests
        testloggerdurability.cpp
        ../../src/logger.cpp
        ../../src/logger.h
        )
set_target_properties(CrashLoggingTests PROPERTIES AUTOMOC ON)
target_compile_features(CrashLoggingTests PRIVATE cxx_std_17)
target_include_directories(CrashLoggingTests PRIVATE "${CMAKE_CURRENT_LIST_DIR}/../../src")
target_link_libraries(CrashLoggingTests PRIVATE Qt${QT_VERSION_MAJOR}::Core)

if(WIN32)
    target_sources(CrashLoggingTests PRIVATE
            ../../src/windowscrashhandler.cpp
            ../../src/windowscrashhandler.h
            )
    target_link_libraries(CrashLoggingTests PRIVATE dbghelp)
endif()

add_test(NAME CrashLoggingTests COMMAND CrashLoggingTests)
set_tests_properties(CrashLoggingTests PROPERTIES TIMEOUT 20)
