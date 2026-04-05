find_package(absl CONFIG REQUIRED)

add_executable(grpcmmo_account_tool
  src/main.cpp
)

target_compile_features(grpcmmo_account_tool PRIVATE cxx_std_23)
target_link_libraries(grpcmmo_account_tool
  PRIVATE
    absl::flags
    absl::flags_parse
    grpcmmo_shared
    grpcmmo_storage
)
