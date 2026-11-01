include(GoogleTest)

add_executable(csms_tests
  test_enums.cpp
  test_time_utils.cpp
  test_database.cpp
  test_validation.cpp
  test_client_repository.cpp
  test_client_service.cpp
  test_case_workflow.cpp
  test_staff_repository.cpp
  test_case_repository.cpp
  test_case_service.cpp
  test_case_history.cpp
  test_password_hasher.cpp
  test_staff_service.cpp
  test_auth_service.cpp
  test_authorization.cpp
  test_reports.cpp
  test_cli.cpp
)
target_link_libraries(csms_tests PRIVATE csms_core GTest::gtest_main)
gtest_discover_tests(csms_tests)
