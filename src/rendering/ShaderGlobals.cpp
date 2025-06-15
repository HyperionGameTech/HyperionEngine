/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <rendering/ShaderGlobals.hpp>
#include <rendering/RenderWorld.hpp>
#include <rendering/RenderScene.hpp>
#include <rendering/RenderCamera.hpp>
#include <rendering/RenderSkeleton.hpp>
#include <rendering/RenderLight.hpp>
#include <rendering/RenderMaterial.hpp>
#include <rendering/RenderEnvProbe.hpp>
#include <rendering/RenderShadowMap.hpp>
#include <rendering/RenderEnvGrid.hpp>
#include <rendering/lightmapper/RenderLightmapVolume.hpp>

#include <rendering/backend/RenderObject.hpp>
#include <rendering/backend/RendererComputePipeline.hpp>
#include <rendering/backend/RendererShader.hpp>
#include <rendering/backend/RendererImage.hpp>

#include <core/logging/LogChannels.hpp>
#include <core/logging/Logger.hpp>

#include <core/profiling/ProfileScope.hpp>

#include <Engine.hpp>

namespace hyperion {

ShaderGlobals::ShaderGlobals()
{
    worlds = g_engine->GetGPUBufferHolderMap()->GetOrCreate<WorldShaderData, GPUBufferType::CONSTANT_BUFFER>();
    cameras = g_engine->GetGPUBufferHolderMap()->GetOrCreate<CameraShaderData, GPUBufferType::CONSTANT_BUFFER>();
    lights = g_engine->GetGPUBufferHolderMap()->GetOrCreate<LightShaderData, GPUBufferType::STORAGE_BUFFER>();
    objects = g_engine->GetGPUBufferHolderMap()->GetOrCreate<EntityShaderData, GPUBufferType::STORAGE_BUFFER>();
    materials = g_engine->GetGPUBufferHolderMap()->GetOrCreate<MaterialShaderData, GPUBufferType::STORAGE_BUFFER>();
    skeletons = g_engine->GetGPUBufferHolderMap()->GetOrCreate<SkeletonShaderData, GPUBufferType::STORAGE_BUFFER>();
    shadow_map_data = g_engine->GetGPUBufferHolderMap()->GetOrCreate<ShadowMapShaderData, GPUBufferType::STORAGE_BUFFER>();
    env_probes = g_engine->GetGPUBufferHolderMap()->GetOrCreate<EnvProbeShaderData, GPUBufferType::STORAGE_BUFFER>();
    env_grids = g_engine->GetGPUBufferHolderMap()->GetOrCreate<EnvGridShaderData, GPUBufferType::CONSTANT_BUFFER>();
    lightmap_volumes = g_engine->GetGPUBufferHolderMap()->GetOrCreate<LightmapVolumeShaderData, GPUBufferType::STORAGE_BUFFER>();
}

void ShaderGlobals::Create()
{
    textures.Create();
}

void ShaderGlobals::Destroy()
{
    textures.Destroy();
}

} // namespace hyperion
