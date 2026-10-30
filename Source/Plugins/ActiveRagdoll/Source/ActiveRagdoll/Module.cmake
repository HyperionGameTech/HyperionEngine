# the ragdoll drives Jolt directly, through the engine's exported Jolt, and reads humanoid mappings from AnimToolkit
target_link_libraries(${plugin_target} PRIVATE $<COMPILE_ONLY:Jolt::Jolt>)

target_include_directories(${plugin_target} PRIVATE
  "${HYP_PLUGIN_SOURCE_ROOT_Animation}"
  "${HYP_GENERATED_DIR}/Plugins/Animation")

if(HYP_DYNAMIC_PLUGINS)
  target_link_libraries(${plugin_target} PRIVATE hyperion-plugin-Animation)
endif()
