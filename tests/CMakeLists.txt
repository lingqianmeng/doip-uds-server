if(NOT TARGET GTest::gtest)
  message(FATAL_ERROR "GoogleTest is required for DOIP_BUILD_TESTS=ON")
endif()

include(GoogleTest)

add_executable(unit_tests
  unit/test_version.cpp
)
target_link_libraries(unit_tests PRIVATE doip_uds_core project_warnings GTest::gtest GTest::gtest_main)
gtest_discover_tests(unit_tests)
