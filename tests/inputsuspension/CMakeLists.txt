cmake_minimum_required(VERSION 3.16)
find_package(Qt${QT_VERSION_MAJOR} COMPONENTS Test REQUIRED)

# Use the real input pipeline and its controls. The application main is omitted;
# its already-generated moc/UI/resource sources are shared with this test target.
set(input_suspension_sources)
foreach(source IN LISTS antimicrox_SOURCES antimicrox_HEADERS_MOC antimicrox_FORMS_HEADERS antimicrox_RESOURCES_RCC)
    if(IS_ABSOLUTE "${source}")
        list(APPEND input_suspension_sources "${source}")
    else()
        list(APPEND input_suspension_sources "${CMAKE_SOURCE_DIR}/${source}")
    endif()
endforeach()
set_source_files_properties(${input_suspension_sources} PROPERTIES SKIP_AUTOMOC ON)

# Current production builds use CMAKE_AUTOMOC rather than QT_WRAP_CPP, so
# antimicrox_HEADERS_MOC is empty and its metaobjects live in this generated unit.
get_target_property(input_suspension_app_automoc antimicrox AUTOMOC)
if(input_suspension_app_automoc)
    get_target_property(input_suspension_app_autogen_dir antimicrox AUTOGEN_BUILD_DIR)
    if(NOT input_suspension_app_autogen_dir)
        set(input_suspension_app_autogen_dir "${CMAKE_BINARY_DIR}/antimicrox_autogen")
    endif()
    if(CMAKE_CONFIGURATION_TYPES)
        set(input_suspension_app_moc "${input_suspension_app_autogen_dir}/mocs_compilation_$<CONFIG>.cpp")
    else()
        set(input_suspension_app_moc "${input_suspension_app_autogen_dir}/mocs_compilation.cpp")
    endif()
    set_source_files_properties("${input_suspension_app_moc}" PROPERTIES GENERATED TRUE SKIP_AUTOMOC ON)
    list(APPEND input_suspension_sources "${input_suspension_app_moc}")
endif()

add_executable(InputSuspensionTests testinputsuspension.cpp ${input_suspension_sources})
add_dependencies(InputSuspensionTests antimicrox)
set_target_properties(InputSuspensionTests PROPERTIES AUTOMOC ON)
target_compile_features(InputSuspensionTests PRIVATE cxx_std_17)
target_include_directories(InputSuspensionTests PRIVATE "${CMAKE_SOURCE_DIR}/src" ${SDL2_INCLUDE_DIRS}/SDL2)
target_link_libraries(InputSuspensionTests PRIVATE
        Qt${QT_VERSION_MAJOR}::Test ${QT_LIBS} ${WIN_LIBS} ${X11_LIBS} ${SDL2_LIBRARIES} ${EXTRA_LIBS})

if(WIN32)
    get_target_property(input_suspension_qt_core Qt${QT_VERSION_MAJOR}::Core LOCATION)
    get_filename_component(input_suspension_qt_bin "${input_suspension_qt_core}" DIRECTORY)
    add_test(NAME InputSuspensionTests COMMAND ${CMAKE_COMMAND} -E env
            "QT_QPA_PLATFORM=offscreen" "QT_PLUGIN_PATH=${QT_PLUGIN_DIR}"
            "PATH=${input_suspension_qt_bin};$ENV{PATH}"
            $<TARGET_FILE:InputSuspensionTests>)
else()
    add_test(NAME InputSuspensionTests COMMAND ${CMAKE_COMMAND} -E env
            "QT_QPA_PLATFORM=offscreen" $<TARGET_FILE:InputSuspensionTests>)
endif()
# A queue that reinserts paused controller events must fail in bounded time.
set_tests_properties(InputSuspensionTests PROPERTIES TIMEOUT 20)
