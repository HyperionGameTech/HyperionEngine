/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <rendering/ShaderGlobals.hpp>
#include <rendering/Scene.hpp>
#include <rendering/Camera.hpp>
#include <rendering/Skeleton.hpp>
#include <rendering/Light.hpp>
#include <rendering/EnvProbe.hpp>
#include <rendering/EnvGrid.hpp>

#include <rendering/backend/RenderObject.hpp>
#include <rendering/backend/RendererComputePipeline.hpp>
#include <rendering/backend/RendererShader.hpp>
#include <rendering/backend/RendererImage.hpp>

#include <core/logging/LogChannels.hpp>
#include <core/logging/Logger.hpp>

#include <core/profiling/ProfileScope.hpp>

#include <Engine.hpp>

namespace hyperion {


#pragma region Render commands

struct RENDER_COMMAND(CreateGlobalSphericalHarmonicsGridBuffer) : renderer::RenderCommand
{
    GPUBufferRef sh_grid_buffer;
    
    RENDER_COMMAND(CreateGlobalSphericalHarmonicsGridBuffer)(GPUBufferRef sh_grid_buffer)
        : sh_grid_buffer(std::move(sh_grid_buffer))
    {
        AssertThrow(this->sh_grid_buffer.IsValid());
    }

    virtual ~RENDER_COMMAND(CreateGlobalSphericalHarmonicsGridBuffer)() = default;

    virtual RendererResult operator()() override
    {
        // @TODO: Make GPU only buffer
        HYPERION_BUBBLE_ERRORS(sh_grid_buffer->Create(g_engine->GetGPUDevice(), sizeof(SHGridBuffer)));
        sh_grid_buffer->Memset(g_engine->GetGPUDevice(), sizeof(SHGridBuffer), 0);

        HYPERION_RETURN_OK;
    }
};

struct RENDER_COMMAND(CreateGlobalSphericalHarmonicsGridImages) : renderer::RenderCommand
{
    FixedArray<GlobalSphericalHarmonicsGrid::GridTexture, 9> grid_textures;
    
    RENDER_COMMAND(CreateGlobalSphericalHarmonicsGridImages)(const FixedArray<GlobalSphericalHarmonicsGrid::GridTexture, 9> &grid_textures)
        : grid_textures(grid_textures)
    {
    }

    virtual ~RENDER_COMMAND(CreateGlobalSphericalHarmonicsGridImages)() = default;

    virtual RendererResult operator()() override
    {
        for (auto &item : grid_textures) {
            HYPERION_BUBBLE_ERRORS(item.image->Create(g_engine->GetGPUDevice()));
            HYPERION_BUBBLE_ERRORS(item.image_view->Create(g_engine->GetGPUDevice(), item.image));
        }

        HYPERION_RETURN_OK;
    }
};

#pragma endregion Render commands

GlobalSphericalHarmonicsGrid::GlobalSphericalHarmonicsGrid()
{
    sh_grid_buffer = MakeRenderObject<GPUBuffer>(renderer::GPUBufferType::STORAGE_BUFFER);

    {
        const uint32 dimension_cube = 32;//uint32(MathUtil::Ceil(std::cbrt(max_bound_ambient_probes)));
        const Vec3u image_dimensions { dimension_cube, dimension_cube, dimension_cube };

        for (auto &item : textures) {
            item.image = MakeRenderObject<Image>(renderer::StorageImage(
                image_dimensions,
                InternalFormat::RGBA16F,
                ImageType::TEXTURE_TYPE_3D,
                FilterMode::TEXTURE_FILTER_LINEAR,
                FilterMode::TEXTURE_FILTER_LINEAR
            ));

            item.image_view = MakeRenderObject<ImageView>();
        }
    }
}

void GlobalSphericalHarmonicsGrid::Create()
{
    PUSH_RENDER_COMMAND(CreateGlobalSphericalHarmonicsGridBuffer, sh_grid_buffer);
    PUSH_RENDER_COMMAND(CreateGlobalSphericalHarmonicsGridImages, textures);
}

void GlobalSphericalHarmonicsGrid::Destroy()
{
    SafeRelease(std::move(sh_grid_buffer));

    for (auto &item : textures) {
        SafeRelease(std::move(item.image));
        SafeRelease(std::move(item.image_view));
    }
}

ShaderGlobals::ShaderGlobals()
{
    scenes = MakeUnique<GPUBufferHolder<SceneShaderData, GPUBufferType::STORAGE_BUFFER>>(max_scenes);
    cameras = MakeUnique<GPUBufferHolder<CameraShaderData, GPUBufferType::CONSTANT_BUFFER>>(max_cameras);
    lights = MakeUnique<GPUBufferHolder<LightShaderData, GPUBufferType::STORAGE_BUFFER>>(max_lights);
    objects = MakeUnique<GPUBufferHolder<EntityShaderData, GPUBufferType::STORAGE_BUFFER>>(max_entities);
    materials = MakeUnique<GPUBufferHolder<MaterialShaderData, GPUBufferType::STORAGE_BUFFER>>(max_materials);
    skeletons = MakeUnique<GPUBufferHolder<SkeletonShaderData, GPUBufferType::STORAGE_BUFFER>>(max_skeletons);
    shadow_map_data = MakeUnique<GPUBufferHolder<ShadowShaderData, GPUBufferType::STORAGE_BUFFER>>(max_shadow_maps);
    env_probes = MakeUnique<GPUBufferHolder<EnvProbeShaderData, GPUBufferType::STORAGE_BUFFER>>(max_env_probes);
    env_grids = MakeUnique<GPUBufferHolder<EnvGridShaderData, GPUBufferType::CONSTANT_BUFFER>>(max_env_grids);
}

void ShaderGlobals::Create()
{
    auto *device = g_engine->GetGPUDevice();

    textures.Create();

    spherical_harmonics_grid.Create();
}

void ShaderGlobals::Destroy()
{
    auto *device = g_engine->GetGPUDevice();
    
    spherical_harmonics_grid.Destroy();
}

void ShaderGlobals::UpdateBuffers(uint32 frame_index)
{
    auto *device = g_engine->GetGPUDevice();

    scenes->UpdateBuffer(device, frame_index);
    cameras->UpdateBuffer(device, frame_index);
    objects->UpdateBuffer(device, frame_index);
    materials->UpdateBuffer(device, frame_index);
    skeletons->UpdateBuffer(device, frame_index);
    lights->UpdateBuffer(device, frame_index);
    shadow_map_data->UpdateBuffer(device, frame_index);
    env_probes->UpdateBuffer(device, frame_index);
    env_grids->UpdateBuffer(device, frame_index);
}

} // namespace hyperion
