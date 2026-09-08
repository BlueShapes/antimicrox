cmake_minimum_required(VERSION 3.16)

project(antimicrox_security_policy_tests LANGUAGES CXX)

enable_testing()

add_executable(SecurityPolicyTests testsecuritypolicy.cpp)
target_compile_features(SecurityPolicyTests PRIVATE cxx_std_17)
target_include_directories(SecurityPolicyTests PRIVATE "${CMAKE_CURRENT_LIST_DIR}/../../src")

add_test(NAME SecurityPolicyTests COMMAND SecurityPolicyTests)
