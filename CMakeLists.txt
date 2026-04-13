cmake_minimum_required(VERSION 3.21)

project(grpcMMO LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(CTest)

if(MSVC)
  add_compile_options(/MP)
endif()

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

include(GrpcMMOWorkspace)
grpcmmo_configure_workspace()

option(GRPCMMO_BUILD_STORAGE_SERVER "Build the optional storage service placeholder." ON)
option(GRPCMMO_BUILD_CLIENT "Build the workspace-aware client stub." ON)

set(GRPCMMO_BUILD_TESTING ${BUILD_TESTING})

if(GRPCMMO_BUILD_CLIENT AND GRPCMMO_HAVE_FRAME_REPO AND NOT TARGET Frame)
  set(BUILD_TESTING OFF)
  add_subdirectory(external/frame EXCLUDE_FROM_ALL)
  set(BUILD_TESTING ${GRPCMMO_BUILD_TESTING})
endif()

add_subdirectory(proto)
add_subdirectory(shared)
add_subdirectory(game)
add_subdirectory(storage)
add_subdirectory(tools)
if(GRPCMMO_BUILD_CLIENT)
  add_subdirectory(client)
endif()
add_subdirectory(services/auth_server)
add_subdirectory(services/game_server)

if(GRPCMMO_BUILD_STORAGE_SERVER)
  add_subdirectory(services/storage_server)
endif()

if(GRPCMMO_BUILD_TESTING)
  add_subdirectory(tests)
endif()
