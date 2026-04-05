find_package(absl CONFIG REQUIRED)

add_executable(grpcmmo_game_server
  src/main.cpp
)

target_compile_features(grpcmmo_game_server PRIVATE cxx_std_23)
target_link_libraries(grpcmmo_game_server
  PRIVATE
    absl::flags
    absl::flags_parse
    grpcmmo_proto
    grpcmmo_shared
    grpcmmo_game
    grpcmmo_storage
)
