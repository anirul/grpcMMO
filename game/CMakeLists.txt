add_library(grpcmmo_game
  src/AuthoritativeWorld.cpp
)

target_compile_features(grpcmmo_game PUBLIC cxx_std_23)
target_include_directories(grpcmmo_game PUBLIC include)
target_link_libraries(grpcmmo_game PUBLIC grpcmmo_proto grpcmmo_shared)
