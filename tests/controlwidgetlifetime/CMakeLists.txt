cmake_minimum_required(VERSION 3.16)
project(antimicrox_control_widget_lifetime_tests LANGUAGES CXX)

enable_testing()
if(NOT DEFINED QT_VERSION_MAJOR)
    set(QT_VERSION_MAJOR 6)
endif()
find_package(Qt${QT_VERSION_MAJOR} COMPONENTS Core Widgets REQUIRED)

add_executable(ControlWidgetLifetimeTests
        testcontrolwidgetlifetime.cpp
        ../../src/gui/flashbuttonwidget.cpp
        ../../src/gui/flashbuttonwidget.h
        )
set_target_properties(ControlWidgetLifetimeTests PROPERTIES AUTOMOC ON)
target_compile_features(ControlWidgetLifetimeTests PRIVATE cxx_std_17)
target_include_directories(ControlWidgetLifetimeTests PRIVATE "${CMAKE_CURRENT_LIST_DIR}/../../src/gui")
target_link_libraries(ControlWidgetLifetimeTests PRIVATE Qt${QT_VERSION_MAJOR}::Widgets)
add_test(NAME ControlWidgetLifetimeTests COMMAND ControlWidgetLifetimeTests)

# The application build ships the Windows platform plugin beside test executables.
# Point offscreen tests at the matching Qt installation's plugin directory explicitly.
set(control_widget_test_environment "QT_QPA_PLATFORM=offscreen")
if(WIN32)
    if(NOT QT_PLUGIN_DIR)
        get_target_property(control_widget_qt_core Qt${QT_VERSION_MAJOR}::Core LOCATION)
        get_filename_component(control_widget_qt_bin "${control_widget_qt_core}" DIRECTORY)
        find_program(control_widget_qmake NAMES qmake${QT_VERSION_MAJOR} qmake HINTS "${control_widget_qt_bin}")
        if(control_widget_qmake)
            execute_process(COMMAND "${control_widget_qmake}" -query QT_INSTALL_PLUGINS
                    OUTPUT_VARIABLE QT_PLUGIN_DIR OUTPUT_STRIP_TRAILING_WHITESPACE)
        endif()
    endif()
    if(QT_PLUGIN_DIR)
        list(APPEND control_widget_test_environment "QT_QPA_PLATFORM_PLUGIN_PATH=${QT_PLUGIN_DIR}/platforms")
    endif()
endif()
set_tests_properties(ControlWidgetLifetimeTests PROPERTIES
        ENVIRONMENT "${control_widget_test_environment}"
        TIMEOUT 10)
