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

    GPUBufferHolderBase             *scenes;
    GPUBufferHolderBase             *cameras;
    GPUBufferHolderBase             *lights;
    GPUBufferHolderBase             *objects;
    GPUBufferHolderBase             *materials;
    GPUBufferHolderBase             *skeletons;
    GPUBufferHolderBase             *shadow_map_data;
    GPUBufferHolderBase             *env_probes;
    GPUBufferHolderBase             *env_grids;
    
    BindlessStorage                 textures;

    GlobalSphericalHarmonicsGrid    spherical_harmonics_grid;
};

} // namespace hyperion

#endif