find_package(GTest CONFIG REQUIRED)

add_executable(grpcmmo_client_tests
  client/ClientHarnessTests.cpp
)

target_compile_features(grpcmmo_client_tests PRIVATE cxx_std_23)
target_link_libraries(grpcmmo_client_tests
  PRIVATE
    GTest::gtest_main
    grpcmmo_client_lib
)

add_test(
  NAME grpcmmo_client_tests
  COMMAND $<TARGET_FILE:grpcmmo_client_tests>
)
set_tests_properties(grpcmmo_client_tests PROPERTIES
  LABELS "grpcmmo"
  WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)

add_executable(grpcmmo_game_tests
  game/AuthoritativeWorldTests.cpp
)

target_compile_features(grpcmmo_game_tests PRIVATE cxx_std_23)
target_link_libraries(grpcmmo_game_tests
  PRIVATE
    GTest::gtest_main
    grpcmmo_game
)

add_test(
  NAME grpcmmo_game_tests
  COMMAND $<TARGET_FILE:grpcmmo_game_tests>
)
set_tests_properties(grpcmmo_game_tests PROPERTIES
  LABELS "grpcmmo"
  WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)
