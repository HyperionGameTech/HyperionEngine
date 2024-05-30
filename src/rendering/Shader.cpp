/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <rendering/Shader.hpp>
#include <rendering/ShaderGlobals.hpp>

#include <core/logging/LogChannels.hpp>
#include <core/logging/Logger.hpp>

#include <Engine.hpp>
#include <util/MiscUtil.hpp>

namespace hyperion {

using renderer::Result;

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

    virtual Result operator()() override
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

    virtual Result operator()() override
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
        const uint dimension_cube = 32;//uint(MathUtil::Ceil(std::cbrt(max_bound_ambient_probes)));
        const Extent3D image_dimensions { dimension_cube, dimension_cube, dimension_cube };

        for (auto &item : textures) {
            item.image = MakeRenderObject<Image>(StorageImage(
                image_dimensions,
                InternalFormat::RGBA16F,
                ImageType::TEXTURE_TYPE_3D,
                FilterMode::TEXTURE_FILTER_LINEAR,
                FilterMode::TEXTURE_FILTER_LINEAR,
                UniquePtr<MemoryStreamedData>::Construct(ByteBuffer(image_dimensions.Size() * sizeof(float) * 4))
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
    
}

void ShaderGlobals::Create()
{
    auto *device = g_engine->GetGPUDevice();

    scenes.Create(device);
    cameras.Create(device);
    materials.Create(device);
    objects.Create(device);
    skeletons.Create(device);
    lights.Create(device);
    shadow_map_data.Create(device);
    env_probes.Create(device);
    env_grids.Create(device);
    immediate_draws.Create(device);
    entity_instance_batches.Create(device);

    textures.Create();

    spherical_harmonics_grid.Create();
}

void ShaderGlobals::Destroy()
{
    auto *device = g_engine->GetGPUDevice();
    
    env_probes.Destroy(device);
    env_grids.Destroy(device);

    scenes.Destroy(device);
    objects.Destroy(device);
    materials.Destroy(device);
    skeletons.Destroy(device);
    lights.Destroy(device);
    shadow_map_data.Destroy(device);
    immediate_draws.Destroy(device);
    entity_instance_batches.Destroy(device);

    spherical_harmonics_grid.Destroy();
}

// ShaderManagerSystem

ShaderManagerSystem *ShaderManagerSystem::GetInstance()
{
    return g_shader_manager;
}

ShaderRef ShaderManagerSystem::GetOrCreate(const ShaderDefinition &definition)
{
    const auto EnsureContainsProperties = [](const ShaderProperties &expected, const ShaderProperties &received) -> bool
    {
        if (!received.HasRequiredVertexAttributes(expected.GetRequiredVertexAttributes())) {
            return false;
        }

        for (const ShaderProperty &property : expected.GetPropertySet()) {
            if (!received.Has(property)) {
                return false;
            }
        }

        return true;
    };

    Mutex::Guard guard(m_mutex);

    { // check if exists in cache
        const auto it = m_map.Find(definition);

        if (it != m_map.End()) {
            if (ShaderRef shader = it->second.Lock()) {
                if (EnsureContainsProperties(definition.GetProperties(), shader->GetCompiledShader()->GetProperties())) {
                    return shader;
                } else {
                    HYP_LOG(Shader, LogLevel::ERR,
                        "Loaded shader from cache (Name: {}, Properties: {}) does not contain the requested properties!\n\tRequested: {}",
                        definition.GetName(),
                        shader->GetCompiledShader()->GetProperties().ToString(),
                        definition.GetProperties().ToString()
                    );

                    // remove bad value from cache
                    m_map.Erase(it);
                }
            }
        }
    }

    CompiledShader compiled_shader;

    const bool is_valid_compiled_shader = g_engine->GetShaderCompiler().GetCompiledShader(
        definition.GetName(),
        definition.GetProperties(),
        compiled_shader
    );
    
    AssertThrowMsg(
        is_valid_compiled_shader,
        "Failed to get compiled shader with name %s and props hash %llu!\n",
        definition.GetName().LookupString(),
        definition.GetProperties().GetHashCode().Value()
    );

    HYP_LOG(Shader, LogLevel::DEBUG, "Creating shader '{}'", definition.GetName());

    ShaderRef shader = MakeRenderObject<renderer::Shader>(RC<CompiledShader>(new CompiledShader(std::move(compiled_shader))));
    DeferCreate(shader, g_engine->GetGPUDevice());

    HYP_LOG(Shader, LogLevel::DEBUG, "Shader '{}' created", definition.GetName());

#ifdef HYP_DEBUG_MODE
    AssertThrow(EnsureContainsProperties(definition.GetProperties(), shader->GetCompiledShader()->GetDefinition().GetProperties()));
#endif

    m_map.Set(definition, shader);

    return shader;
}

ShaderRef ShaderManagerSystem::GetOrCreate(
    Name name,
    const ShaderProperties &props
)
{
    return GetOrCreate(ShaderDefinition {
        name,
        props
    });
}

} // namespace hyperion
