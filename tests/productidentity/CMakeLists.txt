cmake_minimum_required(VERSION 3.16)

project(antimicrox_delta_product_identity_tests LANGUAGES CXX)

enable_testing()

find_package(Qt${QT_VERSION_MAJOR} COMPONENTS Core REQUIRED)

add_executable(ProductIdentityTests
        testproductidentity.cpp
        ../../src/productidentity.cpp
        ../../src/settingsmigration.cpp
        )
set_target_properties(ProductIdentityTests PROPERTIES AUTOMOC OFF)
target_compile_features(ProductIdentityTests PRIVATE cxx_std_17)
target_include_directories(ProductIdentityTests PRIVATE
        "${CMAKE_CURRENT_LIST_DIR}/../../src"
        "${CMAKE_BINARY_DIR}"
        )
target_link_libraries(ProductIdentityTests PRIVATE Qt${QT_VERSION_MAJOR}::Core)

add_test(NAME ProductIdentityTests COMMAND ProductIdentityTests)
