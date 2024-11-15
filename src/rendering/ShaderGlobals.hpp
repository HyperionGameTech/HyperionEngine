/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_SHADER_GLOBALS_HPP
#define HYPERION_SHADER_GLOBALS_HPP

#include <core/containers/HashMap.hpp>
#include <core/containers/TypeMap.hpp>

#include <core/memory/UniquePtr.hpp>

#include <core/threading/DataRaceDetector.hpp>

#include <rendering/Buffers.hpp>
#include <rendering/Bindless.hpp>

#include <rendering/backend/RenderObject.hpp>
#include <rendering/backend/RendererShader.hpp>
#include <rendering/backend/RendererBuffer.hpp>

namespace hyperion {

using renderer::Shader;
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
    void UpdateBuffers(uint32 frame_index);

    GPUBufferHolder<SceneShaderData, GPUBufferType::STORAGE_BUFFER, max_scenes>                         scenes;
    GPUBufferHolder<CameraShaderData, GPUBufferType::CONSTANT_BUFFER, max_cameras>                      cameras;
    GPUBufferHolder<LightShaderData, GPUBufferType::STORAGE_BUFFER, max_lights>                         lights;
    GPUBufferHolder<EntityShaderData, GPUBufferType::STORAGE_BUFFER, max_entities>                      objects;
    GPUBufferHolder<MaterialShaderData, GPUBufferType::STORAGE_BUFFER, max_materials>                   materials;
    GPUBufferHolder<SkeletonShaderData, GPUBufferType::STORAGE_BUFFER, max_skeletons>                   skeletons;
    GPUBufferHolder<ShadowShaderData, GPUBufferType::STORAGE_BUFFER, max_shadow_maps>                   shadow_map_data;
    GPUBufferHolder<EnvProbeShaderData, GPUBufferType::STORAGE_BUFFER, max_env_probes>                  env_probes;
    GPUBufferHolder<EnvGridShaderData, GPUBufferType::CONSTANT_BUFFER, max_env_grids>                   env_grids;
    // GPUBufferHolder<EntityInstanceBatch, GPUBufferType::STORAGE_BUFFER, max_entity_instance_batches>    entity_instance_batches;
    
    BindlessStorage                         textures;

    GlobalSphericalHarmonicsGrid            spherical_harmonics_grid;
};

} // namespace hyperion

#endif