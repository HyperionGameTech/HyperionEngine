/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_SHADER_GLOBALS_HPP
#define HYPERION_SHADER_GLOBALS_HPP

#include <rendering/Buffers.hpp>
#include <rendering/Bindless.hpp>

#include <rendering/backend/RenderObject.hpp>
#include <rendering/backend/RendererShader.hpp>
#include <rendering/backend/RendererBuffer.hpp>

namespace hyperion {

using renderer::ShaderProgram;
using renderer::ShaderObject;
using renderer::ShaderModule;
using renderer::StorageBuffer;
using renderer::UniformBuffer;
using renderer::GPUBufferType;

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

    ShaderData<SceneShaderData, GPUBufferType::STORAGE_BUFFER, max_scenes>                      scenes;
    ShaderData<CameraShaderData, GPUBufferType::CONSTANT_BUFFER, max_cameras>                   cameras;
    ShaderData<LightShaderData, GPUBufferType::STORAGE_BUFFER, max_lights>                      lights;
    ShaderData<EntityShaderData, GPUBufferType::STORAGE_BUFFER, max_entities>                   objects;
    ShaderData<MaterialShaderData, GPUBufferType::STORAGE_BUFFER, max_materials>                materials;
    ShaderData<SkeletonShaderData, GPUBufferType::STORAGE_BUFFER, max_skeletons>                skeletons;
    ShaderData<ShadowShaderData, GPUBufferType::STORAGE_BUFFER, max_shadow_maps>                shadow_map_data;
    ShaderData<EnvProbeShaderData, GPUBufferType::STORAGE_BUFFER, max_env_probes>               env_probes;
    ShaderData<EnvGridShaderData, GPUBufferType::CONSTANT_BUFFER, max_env_grids>                env_grids;
    ShaderData<ImmediateDrawShaderData, GPUBufferType::STORAGE_BUFFER, max_immediate_draws>     immediate_draws;
    ShaderData<EntityInstanceBatch, GPUBufferType::STORAGE_BUFFER, max_entity_instance_batches> entity_instance_batches;
    
    BindlessStorage textures;

    GlobalSphericalHarmonicsGrid spherical_harmonics_grid;
};

} // namespace hyperion

#endif