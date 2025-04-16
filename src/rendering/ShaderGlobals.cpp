/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <rendering/ShaderGlobals.hpp>
#include <rendering/RenderScene.hpp>
#include <rendering/RenderCamera.hpp>
#include <rendering/RenderSkeleton.hpp>
#include <rendering/RenderLight.hpp>
#include <rendering/RenderMaterial.hpp>
#include <rendering/RenderEnvProbe.hpp>
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
        HYPERION_BUBBLE_ERRORS(sh_grid_buffer->Create(sizeof(SHGridBuffer)));
        sh_grid_buffer->Memset(sizeof(SHGridBuffer), 0);

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
            HYPERION_BUBBLE_ERRORS(item.image->Create());
            HYPERION_BUBBLE_ERRORS(item.image_view->Create(item.image));
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
            item.image = MakeRenderObject<Image>(TextureDesc {
                ImageType::TEXTURE_TYPE_3D,
                InternalFormat::RGBA16F,
                image_dimensions,
                FilterMode::TEXTURE_FILTER_LINEAR,
                FilterMode::TEXTURE_FILTER_LINEAR,
                WrapMode::TEXTURE_WRAP_CLAMP_TO_EDGE,
                1,
                ImageFormatCapabilities::SAMPLED | ImageFormatCapabilities::STORAGE
            });

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
    scenes = g_engine->GetGPUBufferHolderMap()->GetOrCreate<SceneShaderData, GPUBufferType::STORAGE_BUFFER>(max_scenes);
    cameras = g_engine->GetGPUBufferHolderMap()->GetOrCreate<CameraShaderData, GPUBufferType::CONSTANT_BUFFER>(max_cameras);
    lights = g_engine->GetGPUBufferHolderMap()->GetOrCreate<LightShaderData, GPUBufferType::STORAGE_BUFFER>(max_lights);
    objects = g_engine->GetGPUBufferHolderMap()->GetOrCreate<EntityShaderData, GPUBufferType::STORAGE_BUFFER>(max_entities);
    materials = g_engine->GetGPUBufferHolderMap()->GetOrCreate<MaterialShaderData, GPUBufferType::STORAGE_BUFFER>(max_materials);
    skeletons = g_engine->GetGPUBufferHolderMap()->GetOrCreate<SkeletonShaderData, GPUBufferType::STORAGE_BUFFER>(max_skeletons);
    shadow_map_data = g_engine->GetGPUBufferHolderMap()->GetOrCreate<ShadowShaderData, GPUBufferType::STORAGE_BUFFER>(max_shadow_maps);
    env_probes = g_engine->GetGPUBufferHolderMap()->GetOrCreate<EnvProbeShaderData, GPUBufferType::STORAGE_BUFFER>(max_env_probes);
    env_grids = g_engine->GetGPUBufferHolderMap()->GetOrCreate<EnvGridShaderData, GPUBufferType::CONSTANT_BUFFER>(max_env_grids);
}

void ShaderGlobals::Create()
{
    textures.Create();

    spherical_harmonics_grid.Create();
}

void ShaderGlobals::Destroy()
{
    spherical_harmonics_grid.Destroy();
}

} // namespace hyperion
