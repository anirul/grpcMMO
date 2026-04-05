set(GRPCMMO_SHARED_GENERATED_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated")

configure_file(
  include/grpcmmo/shared/WorkspaceConfig.hpp.in
  "${GRPCMMO_SHARED_GENERATED_DIR}/grpcmmo/shared/WorkspaceConfig.hpp"
  @ONLY
)

add_library(grpcmmo_shared INTERFACE)

target_compile_features(grpcmmo_shared INTERFACE cxx_std_23)
target_include_directories(grpcmmo_shared
  INTERFACE
    include
    "${GRPCMMO_SHARED_GENERATED_DIR}"
)

if(TARGET grpcmmo_frame_headers)
  target_link_libraries(grpcmmo_shared INTERFACE grpcmmo_frame_headers)
endif()
