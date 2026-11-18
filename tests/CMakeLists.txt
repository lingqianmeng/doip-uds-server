if(NOT TARGET GTest::gtest)
  message(FATAL_ERROR "GoogleTest is required for DOIP_BUILD_TESTS=ON")
endif()

include(GoogleTest)
find_package(Threads REQUIRED)
find_package(Python3 COMPONENTS Interpreter REQUIRED)

add_executable(unit_tests
  unit/test_doip_header.cpp
  unit/test_diagnostic_message.cpp
  unit/test_ecu_identity.cpp
  unit/test_routing_activation.cpp
  unit/test_tcp_transport.cpp
  unit/test_udp_transport.cpp
  unit/test_uds_dispatcher.cpp
  unit/test_vehicle_identification.cpp
  unit/test_vehicle_identification_handler.cpp
  unit/test_version.cpp
)
target_link_libraries(unit_tests PRIVATE doip_uds_core project_warnings GTest::gtest GTest::gtest_main Threads::Threads)
gtest_discover_tests(unit_tests)

add_executable(integration_tests
  integration/test_diagnostic_flow_tcp.cpp
  integration/test_diagnostic_message_tcp.cpp
  integration/test_routing_activation_tcp.cpp
  integration/test_vehicle_identification_udp.cpp
)
target_link_libraries(integration_tests PRIVATE doip_uds_core project_warnings GTest::gtest GTest::gtest_main Threads::Threads)
gtest_discover_tests(integration_tests)

add_test(
  NAME PythonDoipTesterIntegration
  COMMAND ${Python3_EXECUTABLE}
    ${CMAKE_CURRENT_SOURCE_DIR}/integration/test_doip_tester.py
    --server $<TARGET_FILE:doip_server>
    --config ${PROJECT_SOURCE_DIR}/config/ecu_config.json
)
set_tests_properties(PythonDoipTesterIntegration PROPERTIES TIMEOUT 45)
