
# Add source to this project's executable.
add_executable (engine 
  main.cpp
  vk_types.h
  vk_initializers.cpp
  vk_initializers.h
  vk_images.h
  vk_images.cpp 
  vk_descriptors.h
  vk_descriptors.cpp
  vk_pipelines.h
  vk_pipelines.cpp
  vk_engine.h
  vk_engine.cpp
  vk_loader.h
  vk_loader.cpp 
  fish_camera.cpp
  fish_camera.h
  mesh.h
  mesh.cpp
  loader.h
  loader.cpp
  fish_timer.h
  fish_timer.cpp
  fish_resource_manager.h
  fish_resource_manager.cpp
  fish_resource.h
  fish_resource.cpp
  fish_scene.h
  fish_scene.cpp  
  fish_gpu_data.h
  fish_pipeline.h
  fish_pipeline.cpp
  fish_ecs.h
  fish_ecs_systems.h
  fish_ecs_systems.cpp
  nlohmann_json.h
  nlohmann_json_fwd.h
  tiny_obj_loader.h
)

set_property(TARGET engine PROPERTY CXX_STANDARD 20)
target_compile_definitions(engine PUBLIC GLM_FORCE_DEPTH_ZERO_TO_ONE)
target_include_directories(engine PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")

target_link_libraries(engine PUBLIC vma glm Vulkan::Vulkan fmt::fmt stb_image SDL2::SDL2 vkbootstrap imgui fastgltf::fastgltf)

target_precompile_headers(engine PUBLIC <optional> <vector> <memory> <string> <vector> <unordered_map> <glm/mat4x4.hpp>  <glm/vec4.hpp> <vulkan/vulkan.h>)

add_custom_command(TARGET engine POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_RUNTIME_DLLS:engine> $<TARGET_FILE_DIR:engine>
  COMMAND_EXPAND_LISTS
  )