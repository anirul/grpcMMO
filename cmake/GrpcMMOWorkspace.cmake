include_guard(GLOBAL)

function(grpcmmo_configure_workspace)
  get_filename_component(_grpcmmo_workspace_parent "${CMAKE_SOURCE_DIR}" DIRECTORY)
  get_filename_component(_grpcmmo_default_frame_root
                         "${CMAKE_SOURCE_DIR}/external/frame"
                         ABSOLUTE)
  get_filename_component(_grpcmmo_default_data_root
                         "${_grpcmmo_workspace_parent}/grpcMMO-data"
                         ABSOLUTE)

  set(GRPCMMO_FRAME_ROOT
      "${_grpcmmo_default_frame_root}"
      CACHE PATH
            "Workspace path to the sibling Frame repository.")
  set(GRPCMMO_DATA_ROOT
      "${_grpcmmo_default_data_root}"
      CACHE PATH
            "Workspace path to the sibling grpcMMO-data repository.")
  option(GRPCMMO_REQUIRE_FRAME_REPO
         "Fail configuration if the Frame workspace repository is missing."
         OFF)
  option(GRPCMMO_REQUIRE_DATA_REPO
         "Fail configuration if the grpcMMO-data workspace repository is missing."
         OFF)

  file(TO_CMAKE_PATH "${GRPCMMO_FRAME_ROOT}" _grpcmmo_frame_root_cmake)
  file(TO_CMAKE_PATH "${GRPCMMO_DATA_ROOT}" _grpcmmo_data_root_cmake)

  set(_grpcmmo_frame_header "${GRPCMMO_FRAME_ROOT}/frame/api.h")
  set(_grpcmmo_frame_cmake "${GRPCMMO_FRAME_ROOT}/CMakeLists.txt")
  if(EXISTS "${_grpcmmo_frame_header}" AND EXISTS "${_grpcmmo_frame_cmake}")
    set(GRPCMMO_HAVE_FRAME_REPO 1)
  else()
    set(GRPCMMO_HAVE_FRAME_REPO 0)
  endif()

  set(_grpcmmo_data_readme "${GRPCMMO_DATA_ROOT}/README.md")
  set(_grpcmmo_data_attributes "${GRPCMMO_DATA_ROOT}/.gitattributes")
  if(EXISTS "${_grpcmmo_data_readme}" AND EXISTS "${_grpcmmo_data_attributes}")
    set(GRPCMMO_HAVE_DATA_REPO 1)
  else()
    set(GRPCMMO_HAVE_DATA_REPO 0)
  endif()

  if(GRPCMMO_REQUIRE_FRAME_REPO AND NOT GRPCMMO_HAVE_FRAME_REPO)
    message(FATAL_ERROR
            "Frame repository not found at ${GRPCMMO_FRAME_ROOT}. "
            "Set GRPCMMO_FRAME_ROOT to the workspace copy.")
  endif()

  if(GRPCMMO_REQUIRE_DATA_REPO AND NOT GRPCMMO_HAVE_DATA_REPO)
    message(FATAL_ERROR
            "grpcMMO-data repository not found at ${GRPCMMO_DATA_ROOT}. "
            "Set GRPCMMO_DATA_ROOT to the workspace copy.")
  endif()

  set(GRPCMMO_FRAME_ROOT "${GRPCMMO_FRAME_ROOT}" CACHE PATH "" FORCE)
  set(GRPCMMO_DATA_ROOT "${GRPCMMO_DATA_ROOT}" CACHE PATH "" FORCE)
  set(GRPCMMO_FRAME_ROOT_CMAKE "${_grpcmmo_frame_root_cmake}" CACHE INTERNAL "")
  set(GRPCMMO_DATA_ROOT_CMAKE "${_grpcmmo_data_root_cmake}" CACHE INTERNAL "")
  set(GRPCMMO_HAVE_FRAME_REPO "${GRPCMMO_HAVE_FRAME_REPO}" CACHE INTERNAL "")
  set(GRPCMMO_HAVE_DATA_REPO "${GRPCMMO_HAVE_DATA_REPO}" CACHE INTERNAL "")

  if(GRPCMMO_HAVE_FRAME_REPO)
    if(NOT TARGET grpcmmo_frame_headers)
      add_library(grpcmmo_frame_headers INTERFACE)
      target_include_directories(grpcmmo_frame_headers
                                 INTERFACE "${GRPCMMO_FRAME_ROOT}")
    endif()
    message(STATUS "grpcMMO workspace: Frame repository detected at ${GRPCMMO_FRAME_ROOT}")
  else()
    message(STATUS "grpcMMO workspace: Frame repository not found at ${GRPCMMO_FRAME_ROOT}")
  endif()

  if(GRPCMMO_HAVE_DATA_REPO)
    message(STATUS
            "grpcMMO workspace: grpcMMO-data repository detected at ${GRPCMMO_DATA_ROOT}")
  else()
    message(STATUS
            "grpcMMO workspace: grpcMMO-data repository not found at ${GRPCMMO_DATA_ROOT}")
  endif()
endfunction()
