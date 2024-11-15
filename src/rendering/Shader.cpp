/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <rendering/Shader.hpp>
#include <rendering/ShaderGlobals.hpp>

#include <rendering/backend/RendererComputePipeline.hpp>
#include <rendering/backend/RendererShader.hpp>

#include <core/logging/LogChannels.hpp>
#include <core/logging/Logger.hpp>

#include <util/MiscUtil.hpp>

#include <util/profiling/ProfileScope.hpp>

#include <Engine.hpp>

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
    
}

void ShaderGlobals::Create()
{
    auto *device = g_engine->GetGPUDevice();

    // scenes.Create(device);
    // cameras.Create(device);
    // materials.Create(device);
    // objects.Create(device);
    // skeletons.Create(device);
    // lights.Create(device);
    // shadow_map_data.Create(device);
    // env_probes.Create(device);
    // env_grids.Create(device);
    // entity_instance_batches.Create(device);

    textures.Create();

    spherical_harmonics_grid.Create();
}

void ShaderGlobals::Destroy()
{
    auto *device = g_engine->GetGPUDevice();
    
    // env_probes.Destroy(device);
    // env_grids.Destroy(device);

    // scenes.Destroy(device);
    // objects.Destroy(device);
    // materials.Destroy(device);
    // skeletons.Destroy(device);
    // lights.Destroy(device);
    // shadow_map_data.Destroy(device);
    // entity_instance_batches.Destroy(device);

    spherical_harmonics_grid.Destroy();
}

void ShaderGlobals::UpdateBuffers(uint32 frame_index)
{
    auto *device = g_engine->GetGPUDevice();

    scenes.UpdateBuffer(device, frame_index);
    cameras.UpdateBuffer(device, frame_index);
    objects.UpdateBuffer(device, frame_index);
    materials.UpdateBuffer(device, frame_index);
    skeletons.UpdateBuffer(device, frame_index);
    lights.UpdateBuffer(device, frame_index);
    shadow_map_data.UpdateBuffer(device, frame_index);
    env_probes.UpdateBuffer(device, frame_index);
    env_grids.UpdateBuffer(device, frame_index);
}

// ShaderManagerSystem

ShaderManagerSystem *ShaderManagerSystem::GetInstance()
{
    return g_shader_manager;
}

ShaderRef ShaderManagerSystem::GetOrCreate(const ShaderDefinition &definition)
{
    HYP_NAMED_SCOPE("Get shader from cache or create");

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

    RC<ShaderMapEntry> entry;

    { // pulling value from cache if it exists
        Mutex::Guard guard(m_mutex);

        auto it = m_map.Find(definition);

        if (it != m_map.End()) {
            entry = it->second;
            AssertThrow(entry != nullptr);
        }
    }

    if (entry != nullptr) {
        // loading from another thread -- wait until state is no longer LOADING
        while (entry->state.Get(MemoryOrder::SEQUENTIAL) == ShaderMapEntry::State::LOADING) {
            // sanity check - should never happen
            AssertThrow(entry->loading_thread_id != Threads::CurrentThreadID());
        }

        if (ShaderRef shader = entry->shader.Lock()) {
            if (EnsureContainsProperties(definition.GetProperties(), shader->GetCompiledShader()->GetProperties())) {
                return shader;
            } else {
                HYP_LOG(Shader, LogLevel::ERR,
                    "Loaded shader from cache (Name: {}, Properties: {}) does not contain the requested properties!\n\tRequested: {}",
                    definition.GetName(),
                    shader->GetCompiledShader()->GetProperties().ToString(),
                    definition.GetProperties().ToString()
                );

            }
        }

        // @TODO: remove bad value from cache?
    }

    {// lock here, to add new entry to cache
        Mutex::Guard guard(m_mutex);
        
        entry = MakeRefCountedPtr<ShaderMapEntry>();
        entry->state.Set(ShaderMapEntry::State::LOADING, MemoryOrder::SEQUENTIAL);
        entry->loading_thread_id = Threads::CurrentThreadID();

        m_map.Set(definition, entry);
    }

    ShaderRef shader;

    { // loading / compilation of shader (outside of mutex lock)
        CompiledShader compiled_shader;

        bool is_valid_compiled_shader = true;
        
        is_valid_compiled_shader &= g_engine->GetShaderCompiler().GetCompiledShader(
            definition.GetName(),
            definition.GetProperties(),
            compiled_shader
        );

        is_valid_compiled_shader &= compiled_shader.GetDefinition().IsValid();
        
        AssertThrowMsg(
            is_valid_compiled_shader,
            "Failed to get compiled shader with name %s and props hash %llu!\n",
            definition.GetName().LookupString(),
            definition.GetProperties().GetHashCode().Value()
        );

        HYP_LOG(Shader, LogLevel::DEBUG, "Creating shader '{}'", definition.GetName());

        shader = MakeRenderObject<Shader>(MakeRefCountedPtr<CompiledShader>(std::move(compiled_shader)));

        HYP_LOG(Shader, LogLevel::DEBUG, "Shader '{}' created", definition.GetName());

#ifdef HYP_DEBUG_MODE
        AssertThrow(EnsureContainsProperties(definition.GetProperties(), shader->GetCompiledShader()->GetDefinition().GetProperties()));
#endif


        DeferCreate(shader, g_engine->GetGPUDevice());

        // Update the entry
        entry->shader = shader;
        entry->state.Set(ShaderMapEntry::State::LOADED, MemoryOrder::SEQUENTIAL);
    }

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
