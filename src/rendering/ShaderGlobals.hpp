#ifndef HYPERION_V2_SHADER_GLOBALS_H
#define HYPERION_V2_SHADER_GLOBALS_H

#include "Buffers.hpp"
#include "Bindless.hpp"
#include "DrawProxy.hpp"

#include <core/lib/Queue.hpp>
#include <core/ID.hpp>

#include <math/MathUtil.hpp>

#include <rendering/backend/RenderObject.hpp>
#include <rendering/backend/RendererShader.hpp>
#include <rendering/backend/RendererBuffer.hpp>
#include <rendering/backend/RendererStructs.hpp>

#include <utility>

namespace hyperion::v2 {

using renderer::ShaderProgram;
using renderer::ShaderObject;
using renderer::ShaderModule;
using renderer::GPUBuffer;
using renderer::UniformBuffer;
using renderer::StorageBuffer;
using renderer::IndirectBuffer;

class Engine;
class Entity;

struct GlobalSphericalHarmonicsGrid
{
    struct GridTexture
    {
        ImageRef image;
        ImageViewRef image_view;
    };

    FixedArray<GridTexture, 9> textures;

    GPUBufferRef sh_grid_buffer;
    FixedArray<GridTexture, 9> clipmaps;

    GlobalSphericalHarmonicsGrid();

    void Create();
    void Destroy();
};

struct ShaderGlobals
{
    ShaderGlobals();
    ShaderGlobals(const ShaderGlobals &other) = delete;
    ShaderGlobals &operator=(const ShaderGlobals &other) = delete;

    void Create();
    void Destroy();

    ShaderData<StorageBuffer, SceneShaderData, max_scenes> scenes;
    ShaderData<UniformBuffer, CameraShaderData, max_cameras> cameras;
    ShaderData<StorageBuffer, LightShaderData, max_lights> lights;
    ShaderData<StorageBuffer, ObjectShaderData, max_entities> objects;
    ShaderData<StorageBuffer, MaterialShaderData, max_materials> materials;
    ShaderData<StorageBuffer, SkeletonShaderData, max_skeletons> skeletons;
    ShaderData<StorageBuffer, ShadowShaderData, max_shadow_maps> shadow_map_data;
    ShaderData<StorageBuffer, EnvProbeShaderData, max_env_probes> env_probes;
    ShaderData<UniformBuffer, EnvGridShaderData, max_env_grids> env_grids;
    ShaderData<StorageBuffer, ImmediateDrawShaderData, max_immediate_draws> immediate_draws;
    ShaderData<StorageBuffer, EntityInstanceBatch, max_entity_instance_batches> entity_instance_batches;
    
    BindlessStorage textures;

    GlobalSphericalHarmonicsGrid spherical_harmonics_grid;
};

} // namespace hyperion::v2

#endif