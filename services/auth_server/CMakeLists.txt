find_package(absl CONFIG REQUIRED)

add_executable(grpcmmo_auth_server
  src/main.cpp
)

target_compile_features(grpcmmo_auth_server PRIVATE cxx_std_23)
target_link_libraries(grpcmmo_auth_server
  PRIVATE
    absl::flags
    absl::flags_parse
    grpcmmo_proto
    grpcmmo_shared
    grpcmmo_storage
)
