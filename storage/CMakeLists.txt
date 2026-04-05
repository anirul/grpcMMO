find_package(unofficial-sqlite3 CONFIG QUIET)
if(TARGET unofficial::sqlite3::sqlite3)
  set(GRPCMMO_SQLITE_TARGET unofficial::sqlite3::sqlite3)
else()
  find_package(SQLite3 REQUIRED)
  set(GRPCMMO_SQLITE_TARGET SQLite::SQLite3)
endif()

add_library(grpcmmo_storage
  src/SqliteStorage.cpp
)

target_compile_features(grpcmmo_storage PUBLIC cxx_std_23)
target_include_directories(grpcmmo_storage PUBLIC include)
target_link_libraries(grpcmmo_storage PUBLIC grpcmmo_shared PRIVATE ${GRPCMMO_SQLITE_TARGET})
