cmake_minimum_required(VERSION 3.16)

project(antimicrox_input_release_latch_tests LANGUAGES CXX)

find_package(Qt${QT_VERSION_MAJOR} COMPONENTS Core Test REQUIRED)

add_executable(InputReleaseLatchTests
        testinputreleaselatch.cpp
        ../../src/inputreleaselatch.cpp
        ../../src/inputreleaselatch.h
        )
set_target_properties(InputReleaseLatchTests PROPERTIES AUTOMOC ON)
target_compile_features(InputReleaseLatchTests PRIVATE cxx_std_17)
target_include_directories(InputReleaseLatchTests PRIVATE "${CMAKE_CURRENT_LIST_DIR}/../../src")
target_link_libraries(InputReleaseLatchTests PRIVATE Qt${QT_VERSION_MAJOR}::Core Qt${QT_VERSION_MAJOR}::Test)
if(WIN32)
    get_target_property(input_latch_qt_core Qt${QT_VERSION_MAJOR}::Core LOCATION)
    get_filename_component(input_latch_qt_bin "${input_latch_qt_core}" DIRECTORY)
    add_test(NAME InputReleaseLatchTests COMMAND ${CMAKE_COMMAND} -E env
            "PATH=${input_latch_qt_bin};$ENV{PATH}" $<TARGET_FILE:InputReleaseLatchTests>)
else()
    add_test(NAME InputReleaseLatchTests COMMAND InputReleaseLatchTests)
endif()
set_tests_properties(InputReleaseLatchTests PROPERTIES TIMEOUT 15)
