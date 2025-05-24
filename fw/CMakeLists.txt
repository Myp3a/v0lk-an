cmake_minimum_required(VERSION 3.25)
project(fw VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(GLSL_VALIDATOR "$ENV{VULKAN_SDK}/bin/glslangValidator")

add_library(fw fw.cpp)
target_include_directories(fw PUBLIC "${PROJECT_BINARY_DIR}" "${PROJECT_SOURCE_DIR}")

include(cmake/CPM.cmake)

find_package(Vulkan)
target_link_libraries(fw Vulkan::Vulkan)

CPMAddPackage("gh:glfw/glfw#3.4")
target_link_libraries(fw glfw)

CPMAddPackage("gh:g-truc/glm#1.0.1")
target_link_libraries(fw glm)

file(GLOB_RECURSE GLSL_SOURCE_FILES "shaders/*.frag" "shaders/*.vert")
foreach(GLSL ${GLSL_SOURCE_FILES})
message(STATUS GLSL)
  get_filename_component(FILE_NAME ${GLSL} NAME)
  set(SPIRV "${PROJECT_BINARY_DIR}/shaders/${FILE_NAME}.spv")
  add_custom_command(
    OUTPUT ${SPIRV}
    COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_BINARY_DIR}/shaders/"
    COMMAND ${GLSL_VALIDATOR} -V ${GLSL} -o ${SPIRV}
    DEPENDS ${GLSL})
  list(APPEND SPIRV_BINARY_FILES ${SPIRV})
endforeach(GLSL)

add_custom_target(shaders DEPENDS ${SPIRV_BINARY_FILES})

add_dependencies(fw shaders)

add_custom_command(TARGET fw POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:fw>/shaders/"
    COMMAND ${CMAKE_COMMAND} -E copy_directory "${PROJECT_BINARY_DIR}/shaders" "${CMAKE_BINARY_DIR}/shaders"
)