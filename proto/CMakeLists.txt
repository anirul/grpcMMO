find_package(protobuf CONFIG REQUIRED)
find_package(gRPC CONFIG REQUIRED)

set(GRPCMMO_PROTO_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
set(GRPCMMO_PROTO_FILES
  "${GRPCMMO_PROTO_DIR}/auth/v1/auth.proto"
  "${GRPCMMO_PROTO_DIR}/chat/v1/chat.proto"
  "${GRPCMMO_PROTO_DIR}/combat/v1/combat.proto"
  "${GRPCMMO_PROTO_DIR}/session/v1/session.proto"
  "${GRPCMMO_PROTO_DIR}/storage/v1/storage.proto"
  "${GRPCMMO_PROTO_DIR}/world/v1/replication.proto"
)
set(GRPCMMO_SERVICE_PROTO_FILES
  "${GRPCMMO_PROTO_DIR}/auth/v1/auth.proto"
  "${GRPCMMO_PROTO_DIR}/session/v1/session.proto"
)

add_library(grpcmmo_proto STATIC ${GRPCMMO_PROTO_FILES})

if(MSVC)
  target_compile_options(grpcmmo_proto PRIVATE /FS)
endif()

target_link_libraries(grpcmmo_proto
  PUBLIC
    gRPC::grpc++
    protobuf::libprotobuf
)

target_include_directories(grpcmmo_proto
  PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>"
)

protobuf_generate(
  TARGET grpcmmo_proto
  LANGUAGE cpp
  IMPORT_DIRS "${GRPCMMO_PROTO_DIR}"
)

protobuf_generate(
  LANGUAGE grpc
  PROTOS "${GRPCMMO_SERVICE_PROTO_FILES}"
  GENERATE_EXTENSIONS .grpc.pb.h .grpc.pb.cc
  IMPORT_DIRS "${GRPCMMO_PROTO_DIR}"
  PLUGIN "protoc-gen-grpc=$<TARGET_FILE:gRPC::grpc_cpp_plugin>"
  OUT_VAR GRPCMMO_GRPC_GENERATED
)

target_sources(grpcmmo_proto PRIVATE ${GRPCMMO_GRPC_GENERATED})
