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

class Engine;
class Entity;

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
};

} // namespace hyperion

#endif