#ifndef HYPERION_V2_SHADER_GLOBALS_H
#define HYPERION_V2_SHADER_GLOBALS_H

#include "buffers.h"
#include "bindless.h"

#include <rendering/backend/renderer_shader.h>
#include <rendering/backend/renderer_buffer.h>
#include <rendering/backend/renderer_structs.h>

#include <memory>
#include <utility>
#include <string>

namespace hyperion::v2 {

using renderer::ShaderProgram;
using renderer::ShaderObject;
using renderer::ShaderModule;
using renderer::GPUBuffer;
using renderer::UniformBuffer;
using renderer::StorageBuffer;

class Engine;

struct ShaderGlobals {
    ShaderGlobals(size_t num_buffers)
        : scenes(num_buffers),
          lights(num_buffers),
          objects(num_buffers),
          materials(num_buffers),
          skeletons(num_buffers),
          shadow_maps(num_buffers)
    {
    }

    ShaderGlobals(const ShaderGlobals &other) = delete;
    ShaderGlobals &operator=(const ShaderGlobals &other) = delete;

    void Create(Engine *engine);
    void Destroy(Engine *engine);

    ShaderData<StorageBuffer, SceneShaderData, max_scenes>        scenes;
    ShaderData<StorageBuffer, LightShaderData, max_lights>        lights;
    ShaderData<StorageBuffer, ObjectShaderData, max_objects>      objects;
    ShaderData<StorageBuffer, MaterialShaderData, max_materials>  materials;
    ShaderData<StorageBuffer, SkeletonShaderData, max_skeletons>  skeletons;
    ShaderData<UniformBuffer, ShadowShaderData, max_shadow_maps>  shadow_maps;
    BindlessStorage                                               textures;
};

} // namespace hyperion::v2

#endif