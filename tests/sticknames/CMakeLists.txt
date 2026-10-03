cmake_minimum_required(VERSION 3.16)

project(antimicrox_delta_stick_name_tests LANGUAGES CXX)

find_package(Qt${QT_VERSION_MAJOR} COMPONENTS Core REQUIRED)

add_executable(StickNameStateTests
        teststicknamestate.cpp
        ../../src/sticknamestate.cpp
        )
set_target_properties(StickNameStateTests PROPERTIES AUTOMOC OFF)
target_compile_features(StickNameStateTests PRIVATE cxx_std_17)
target_include_directories(StickNameStateTests PRIVATE "${CMAKE_CURRENT_LIST_DIR}/../../src")
target_link_libraries(StickNameStateTests PRIVATE Qt${QT_VERSION_MAJOR}::Core)

add_test(NAME StickNameStateTests COMMAND StickNameStateTests)
set_tests_properties(StickNameStateTests PROPERTIES TIMEOUT 10)
