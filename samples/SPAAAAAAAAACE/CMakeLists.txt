add_executable(space space.cpp)

include(../../cmake/compile_shaders.cmake)
include(../../cmake/copy_resources.cmake)

target_link_libraries(space PUBLIC volchara)

define_resource_set(
    NAME space_textures
    GLOB "textures/*"
)
define_resource_set(
    NAME space_models
    GLOB "models/*"
)
use_resource_set(TARGET volchara SETS space_textures)
use_resource_set(TARGET volchara SETS space_models)
use_resource_set(TARGET volchara SETS base_textures)
use_shader_set(TARGET space SETS base_shaders)

# target_compile_definitions(space PRIVATE RESOURCE_DIR="${CMAKE_BINARY_DIR}/resources")
