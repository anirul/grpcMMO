find_package(absl CONFIG REQUIRED)

add_library(grpcmmo_client_frame_deps INTERFACE)

set(GRPCMMO_CLIENT_FRAME_LIBS
  Frame
  FrameFile
  FrameJson
  FrameGui
  FrameOpenGL
  FrameOpenGLFile
  FrameOpenGLGui
  FrameVulkan
  FrameVulkanJson
  FrameVulkanGui
  FrameProto
  FrameCommon
)

if(UNIX AND NOT APPLE AND CMAKE_VERSION VERSION_GREATER_EQUAL 3.24)
  target_link_libraries(grpcmmo_client_frame_deps
    INTERFACE
      "$<LINK_GROUP:RESCAN,${GRPCMMO_CLIENT_FRAME_LIBS}>"
  )
elseif(UNIX AND NOT APPLE)
  target_link_libraries(grpcmmo_client_frame_deps
    INTERFACE
      "-Wl,--start-group"
      ${GRPCMMO_CLIENT_FRAME_LIBS}
      "-Wl,--end-group"
  )
else()
  target_link_libraries(grpcmmo_client_frame_deps
    INTERFACE
      ${GRPCMMO_CLIENT_FRAME_LIBS}
  )
endif()

set(GRPCMMO_CLIENT_SOURCES
  src/AssetBootstrap.cpp
  src/Camera.cpp
  src/CameraBoon.cpp
  src/CameraDirector.cpp
  src/Client.cpp
  src/ClientWorld.cpp
  src/Controller.cpp
  src/FrameSceneBridge.cpp
  src/GrpcSessionClient.cpp
  src/Pawn.cpp
  src/PlayerController.cpp
  src/WorkspacePaths.cpp
)

set(GRPCMMO_CLIENT_HEADERS
  src/Actor.hpp
  src/ActorFactory.hpp
  src/AssetBootstrap.hpp
  src/Camera.hpp
  src/CameraActor.hpp
  src/CameraBoon.hpp
  src/CameraDirector.hpp
  src/Character.hpp
  src/Client.hpp
  src/ClientWorld.hpp
  src/Controller.hpp
  src/FrameSceneBridge.hpp
  src/GrpcSessionClient.hpp
  src/MoveCommand.hpp
  src/Object.hpp
  src/Pawn.hpp
  src/PlayerController.hpp
  src/WorkspacePaths.hpp
  src/WorldActor.hpp
)

set(GRPCMMO_CLIENT_ENTRYPOINT
  src/main.cpp
)

add_library(grpcmmo_client_lib
  ${GRPCMMO_CLIENT_SOURCES}
  ${GRPCMMO_CLIENT_HEADERS}
)

source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}/src" FILES
  ${GRPCMMO_CLIENT_SOURCES}
  ${GRPCMMO_CLIENT_HEADERS}
  ${GRPCMMO_CLIENT_ENTRYPOINT}
)

target_compile_features(grpcmmo_client_lib PUBLIC cxx_std_23)
target_include_directories(grpcmmo_client_lib PUBLIC src)
target_link_libraries(grpcmmo_client_lib
  PUBLIC
    absl::flags
    absl::flags_parse
    grpcmmo_client_frame_deps
    grpcmmo_proto
    grpcmmo_shared
)

add_executable(grpcmmo_client
  ${GRPCMMO_CLIENT_ENTRYPOINT}
)

target_compile_features(grpcmmo_client PRIVATE cxx_std_23)
target_link_libraries(grpcmmo_client PRIVATE grpcmmo_client_lib)
