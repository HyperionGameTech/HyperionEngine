/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <rendering/Shader.hpp>

#include <rendering/backend/RendererComputePipeline.hpp>
#include <rendering/backend/RendererShader.hpp>

#include <core/logging/LogChannels.hpp>
#include <core/logging/Logger.hpp>

#include <core/profiling/ProfileScope.hpp>

#include <Engine.hpp>

namespace hyperion {

ShaderManager *ShaderManager::GetInstance()
{
    return g_shader_manager;
}

ShaderRef ShaderManager::GetOrCreate(const ShaderDefinition &definition)
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
                HYP_LOG(Shader, Error,
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

        shader = g_rendering_api->MakeShader(MakeRefCountedPtr<CompiledShader>(std::move(compiled_shader)));

#ifdef HYP_DEBUG_MODE
        AssertThrow(EnsureContainsProperties(definition.GetProperties(), shader->GetCompiledShader()->GetDefinition().GetProperties()));
#endif

        DeferCreate(shader);

        // Update the entry
        entry->shader = shader;
        entry->state.Set(ShaderMapEntry::State::LOADED, MemoryOrder::SEQUENTIAL);
    }

    return shader;
}

ShaderRef ShaderManager::GetOrCreate(
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
