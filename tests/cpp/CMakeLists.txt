find_package(GTest REQUIRED)

add_executable(silex_core_tests
    test_constants.cpp
    test_utils.cpp
    test_resolver.cpp
)

target_link_libraries(silex_core_tests PRIVATE
    silex_core
    GTest::GTest
    GTest::Main
)

target_include_directories(silex_core_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/source/silex_core/src
)

gtest_discover_tests(silex_core_tests
    DISCOVERY_MODE PRE_TEST
)
