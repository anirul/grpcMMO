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

add_executable(grpcmmo_shared_tests
  shared/PlanetMathTests.cpp
  shared/PreviewPatchTerrainSamplerTests.cpp
)

target_compile_features(grpcmmo_shared_tests PRIVATE cxx_std_23)
target_link_libraries(grpcmmo_shared_tests
  PRIVATE
    GTest::gtest_main
    grpcmmo_shared
)

add_test(
  NAME grpcmmo_shared_tests
  COMMAND $<TARGET_FILE:grpcmmo_shared_tests>
)
set_tests_properties(grpcmmo_shared_tests PROPERTIES
  LABELS "grpcmmo"
  WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)

add_executable(grpcmmo_terrain_tests
  tools/TerrainPatchBakerTests.cpp
)

target_compile_features(grpcmmo_terrain_tests PRIVATE cxx_std_23)
target_link_libraries(grpcmmo_terrain_tests
  PRIVATE
    GTest::gtest_main
    grpcmmo_terrain_tool_lib
)

add_test(
  NAME grpcmmo_terrain_tests
  COMMAND $<TARGET_FILE:grpcmmo_terrain_tests>
)
set_tests_properties(grpcmmo_terrain_tests PROPERTIES
  LABELS "grpcmmo"
  WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)
