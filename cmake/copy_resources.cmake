include_guard(GLOBAL)

function(define_resource_set)
  cmake_parse_arguments(PARSE_ARGV 0 A NO_TREE "NAME;ROOT" "GLOB;FILES")
  if(NOT A_NAME)
    message(FATAL_ERROR "define_resource_set: you must NAME the resource set!")
  endif()

  if (TARGET ${A_NAME})
    message(FATAL_ERROR "define_resource_set: set ${A_NAME} is already defined!")
  endif()

  if(A_ROOT)
    set(_resource_root "${CMAKE_CURRENT_SOURCE_DIR}/${A_ROOT}")
  else()
    set(_resource_root ${CMAKE_CURRENT_SOURCE_DIR})
  endif()

  set(_resource_files ${A_FILES})
  if(A_GLOB)
    foreach(_glob IN LISTS A_GLOB)
      file(GLOB_RECURSE _files CONFIGURE_DEPENDS ${_glob})
      list(APPEND _resource_files ${_files})
    endforeach()
  endif()
  list(REMOVE_DUPLICATES _resource_files)

  set(_copied_files)
  foreach(_resource IN LISTS _resource_files)
    cmake_path(RELATIVE_PATH _resource BASE_DIRECTORY ${_resource_root} OUTPUT_VARIABLE _relative_path)
    if(NOT _relative_path)
      message(FATAL_ERROR "define_resource_set: couldn't find relative path for ${_resource}! (relative to ${_resource_root})")
    endif()
    cmake_path(REMOVE_FILENAME _relative_path OUTPUT_VARIABLE _containing_directory)
    cmake_path(GET _relative_path FILENAME _filename)
    if(A_NO_TREE)
      set(_destination_directory "${CMAKE_BINARY_DIR}/resources/")
      set(_destination_path "${CMAKE_BINARY_DIR}/resources/${_filename}")
    else()
      set(_destination_directory "${CMAKE_BINARY_DIR}/resources/${_containing_directory}")
      set(_destination_path "${CMAKE_BINARY_DIR}/resources/${_containing_directory}${_filename}")
    endif()
    add_custom_command(
      OUTPUT "${_destination_path}"
      COMMAND ${CMAKE_COMMAND} -E make_directory "${_destination_directory}"
      COMMAND ${CMAKE_COMMAND} -E copy "${_resource}" "${_destination_path}"
      COMMENT "Copying resource ${_filename}: ${_resource} -> ${_destination_path}"
      DEPENDS "${_resource}"
    )
    list(APPEND _copied_files "${_destination_path}")
  endforeach()

  add_custom_target(${A_NAME} DEPENDS ${_copied_files})
  target_sources(${A_NAME} PRIVATE ${_resource_files})
endfunction()

function(use_resource_set)
  cmake_parse_arguments(PARSE_ARGV 0 U "" "TARGET" "SETS")
  if(NOT U_TARGET OR NOT U_SETS)
    message(FATAL_ERROR "use_resource_set: you must make TARGET depend on at least one SETS!")
  endif()

  foreach(_resource_set IN LISTS U_SETS)
    if(NOT TARGET ${_resource_set})
      message(FATAL_ERROR "use_resource_set: can't found ${_resource_set} set!")
    endif()
    add_dependencies(${U_TARGET} ${_resource_set})
  endforeach()
endfunction()
