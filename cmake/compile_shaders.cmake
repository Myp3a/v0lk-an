include_guard(GLOBAL)

set(GLSL_VALIDATOR "$ENV{VULKAN_SDK}/bin/glslangValidator")

function(define_shader_set)
  cmake_parse_arguments(PARSE_ARGV 0 A "" "NAME" "INPUTS;GLOB")
  if(NOT A_NAME)
    message(FATAL_ERROR "define_shader_set: you must NAME the shader set!")
  endif()

  if (TARGET ${A_NAME})
    message(FATAL_ERROR "define_shader_set: set ${A_NAME} is already defined!")
  endif()

  set(_raw_shaders ${A_INPUTS})
  if(A_GLOB)
    foreach(_glob IN LISTS A_GLOB)
      file(GLOB_RECURSE _files CONFIGURE_DEPENDS ${_glob})
      list(APPEND _raw_shaders ${_files})
    endforeach()
  endif()
  list(REMOVE_DUPLICATES _raw_shaders)

  set(_compiled_shaders)
  foreach(_raw_shader IN LISTS _raw_shaders)
    cmake_path(GET _raw_shader FILENAME _shader_filename)
    set(_compiled_shader "${CMAKE_BINARY_DIR}/resources/shaders/${_shader_filename}.spv")
    add_custom_command(
      OUTPUT ${_compiled_shader}
      COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_BINARY_DIR}/resources/shaders/"
      # -gVS - debug compilation (RenderDoc debug, for example)
      COMMAND ${GLSL_VALIDATOR} -gVS -V ${_raw_shader} -o ${_compiled_shader}
      COMMENT "Compiling shader ${_shader_filename}: ${_raw_shader} -> ${_compiled_shader}"
      DEPENDS ${_raw_shader})
    list(APPEND _compiled_shaders ${_compiled_shader})
  endforeach()

  add_custom_target(${A_NAME} DEPENDS ${_compiled_shaders})
  target_sources(${A_NAME} PRIVATE ${_raw_shaders})
endfunction()

function(use_shader_set)
  cmake_parse_arguments(PARSE_ARGV 0 A "" "TARGET" "SETS")
  if(NOT A_TARGET OR NOT A_SETS)
    message(FATAL_ERROR "use_shader_set: you must make TARGET depend on at least one SETS!")
  endif()

  foreach(_shader_set IN LISTS A_SETS)
    if(NOT TARGET ${_shader_set})
      message(FATAL_ERROR "use_shader_set: can't found ${_shader_set} set!")
    endif()
    add_dependencies(${A_TARGET} ${_shader_set})
  endforeach()
endfunction()
