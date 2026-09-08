cmake_minimum_required(VERSION 3.16)

project(antimicrox_security_policy_tests LANGUAGES CXX)

enable_testing()

find_package(Qt${QT_VERSION_MAJOR} COMPONENTS Core REQUIRED)

add_executable(SecurityPolicyTests
        testsecuritypolicy.cpp
        ../../src/profilexmlsafety.cpp
        )
set_target_properties(SecurityPolicyTests PROPERTIES AUTOMOC OFF)
target_compile_features(SecurityPolicyTests PRIVATE cxx_std_17)
target_include_directories(SecurityPolicyTests PRIVATE "${CMAKE_CURRENT_LIST_DIR}/../../src")
target_link_libraries(SecurityPolicyTests PRIVATE Qt${QT_VERSION_MAJOR}::Core)

if(WIN32)
    target_sources(SecurityPolicyTests PRIVATE ../../src/windowstokenelevation.cpp)
endif()

add_test(NAME SecurityPolicyTests COMMAND SecurityPolicyTests)
