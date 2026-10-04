cmake_minimum_required(VERSION 3.16)
find_package(Qt${QT_VERSION_MAJOR} COMPONENTS Core Network Test REQUIRED)
add_executable(NativeGameInputTests
        testnativegameinput.cpp
        ../../src/nativegameinputstate.cpp
        ../../src/nativegameinputmonitor.cpp
        ../../src/nativegameinputmonitor.h)
set_target_properties(NativeGameInputTests PROPERTIES AUTOMOC ON)
target_compile_features(NativeGameInputTests PRIVATE cxx_std_17)
target_include_directories(NativeGameInputTests PRIVATE "${CMAKE_CURRENT_LIST_DIR}/../../src")
target_link_libraries(NativeGameInputTests PRIVATE Qt${QT_VERSION_MAJOR}::Core Qt${QT_VERSION_MAJOR}::Network Qt${QT_VERSION_MAJOR}::Test)
if(WIN32)
    target_link_libraries(NativeGameInputTests PRIVATE advapi32 user32)
    add_custom_command(TARGET NativeGameInputTests POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:Qt${QT_VERSION_MAJOR}::Test> $<TARGET_FILE_DIR:NativeGameInputTests>)
endif()
if(WIN32)
    get_target_property(native_game_qt_core Qt${QT_VERSION_MAJOR}::Core LOCATION)
    get_filename_component(native_game_qt_bin "${native_game_qt_core}" DIRECTORY)
    # Supply the matching compiler/Qt runtime even when CTest has a minimal PATH.
    add_test(NAME NativeGameInputTests COMMAND ${CMAKE_COMMAND} -E env
            "PATH=${native_game_qt_bin};$ENV{PATH}" $<TARGET_FILE:NativeGameInputTests>)
else()
    add_test(NAME NativeGameInputTests COMMAND NativeGameInputTests)
endif()
set_tests_properties(NativeGameInputTests PROPERTIES TIMEOUT 15)
