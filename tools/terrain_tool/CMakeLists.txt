find_package(absl CONFIG REQUIRED)
find_package(TIFF REQUIRED)

add_library(grpcmmo_terrain_tool_lib
  src/TerrainPatchBaker.cpp
  src/TerrainPatchBaker.hpp
)

target_compile_features(grpcmmo_terrain_tool_lib PUBLIC cxx_std_23)
target_include_directories(grpcmmo_terrain_tool_lib PUBLIC src)
target_link_libraries(grpcmmo_terrain_tool_lib
  PUBLIC
    grpcmmo_shared
    TIFF::TIFF
)

add_executable(grpcmmo_terrain_tool
  src/main.cpp
)

target_compile_features(grpcmmo_terrain_tool PRIVATE cxx_std_23)
target_link_libraries(grpcmmo_terrain_tool
  PRIVATE
    absl::flags
    absl::flags_parse
    grpcmmo_shared
    grpcmmo_terrain_tool_lib
)
