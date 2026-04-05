find_package(absl CONFIG REQUIRED)

set(GRPCMMO_CLIENT_SOURCES
  src/AssetBootstrap.cpp
  src/Camera.cpp
  src/CameraBoon.cpp
  src/Client.cpp
  src/Controller.cpp
  src/GrpcSessionClient.cpp
  src/Pawn.cpp
  src/Scene.cpp
  src/main.cpp
)

set(GRPCMMO_CLIENT_HEADERS
  src/AssetBootstrap.hpp
  src/Camera.hpp
  src/CameraBoon.hpp
  src/Client.hpp
  src/Controller.hpp
  src/GrpcSessionClient.hpp
  src/MoveCommand.hpp
  src/Object.hpp
  src/Pawn.hpp
  src/Scene.hpp
)

add_executable(grpcmmo_client
  ${GRPCMMO_CLIENT_SOURCES}
  ${GRPCMMO_CLIENT_HEADERS}
)

source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}/src" FILES
  ${GRPCMMO_CLIENT_SOURCES}
  ${GRPCMMO_CLIENT_HEADERS}
)

target_compile_features(grpcmmo_client PRIVATE cxx_std_23)
target_include_directories(grpcmmo_client PRIVATE src)
target_link_libraries(grpcmmo_client
  PRIVATE
    absl::flags
    absl::flags_parse
    Frame
    FrameCommon
    FrameFile
    FrameJson
    FrameOpenGL
    FrameOpenGLFile
    FrameVulkan
    grpcmmo_proto
    grpcmmo_shared
)
