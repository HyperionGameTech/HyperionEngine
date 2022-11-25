#ifndef HYPERION_V2_SHADER_GLOBALS_H
#define HYPERION_V2_SHADER_GLOBALS_H

#include "Buffers.hpp"
#include "Bindless.hpp"
#include "DrawProxy.hpp"

#include <rendering/backend/RendererShader.hpp>
#include <rendering/backend/RendererBuffer.hpp>
#include <rendering/backend/RendererStructs.hpp>

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
using renderer::IndirectBuffer;

class Engine;

struct ShaderGlobals
{
    ShaderGlobals(SizeType num_buffers)
        : scenes(num_buffers),
          lights(num_buffers),
          objects(num_buffers),
          materials(num_buffers),
          skeletons(num_buffers),
          shadow_maps(num_buffers),
          env_probes(num_buffers),
          immediate_draws(num_buffers)
    {
    }

    ShaderGlobals(const ShaderGlobals &other) = delete;
    ShaderGlobals &operator=(const ShaderGlobals &other) = delete;

    void Create();
    void Destroy();

    ShaderData<StorageBuffer, SceneShaderData, max_scenes> scenes;
    ShaderData<StorageBuffer, LightShaderData, max_lights> lights;
    ShaderData<StorageBuffer, ObjectShaderData, max_objects> objects;
    ShaderData<StorageBuffer, MaterialShaderData, max_materials> materials;
    ShaderData<StorageBuffer, SkeletonShaderData, max_skeletons> skeletons;
    ShaderData<UniformBuffer, ShadowShaderData, max_shadow_maps> shadow_maps;
    ShaderData<UniformBuffer, EnvProbeShaderData, max_env_probes> env_probes;
    ShaderData<StorageBuffer, ImmediateDrawShaderData, max_immediate_draws> immediate_draws;
    BindlessStorage textures;

    UniformBuffer cubemap_uniforms;
};

} // namespace hyperion::v2

#endif