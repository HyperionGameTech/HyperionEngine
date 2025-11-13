/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <HyperionPch.hpp>

#include <rendering/ShaderManager.hpp>

#include <rendering/RenderComputePipeline.hpp>
#include <rendering/RenderShader.hpp>
#include <rendering/RenderBackend.hpp>

#include <core/logging/LogChannels.hpp>
#include <core/logging/Logger.hpp>

#include <core/profiling/ProfileScope.hpp>

#include <engine/EngineGlobals.hpp>

namespace hyperion {

ShaderManager* ShaderManager::GetInstance()
{
    return g_shaderManager;
}

ShaderRef ShaderManager::GetOrCreate(const ShaderDefinition& definition)
{
    HYP_NAMED_SCOPE("Get shader from cache or create");

    const auto ensureContainsProperties = [](const ShaderProperties& expected, const ShaderProperties& received) -> bool
    {
        for (const ShaderProperty& property : expected.GetPropertySet())
        {
            if (!received.GetPropertySet().Contains(property))
            {
                return false;
            }
        }

        return true;
    };

    RC<ShaderMapEntry> entry;
    bool shouldAddEntryToCache = false;

    { // pulling value from cache if it exists
        Mutex::Guard guard(m_mutex);

        auto it = m_map.Find(definition);

        if (it != m_map.End())
        {
            entry = it->second;
        }
        else
        {
            shouldAddEntryToCache = true;
        }
    }

    if (entry != nullptr)
    {
        constexpr int maxSpins = 1024;
        int numSpins = 0;

        // loading from another thread -- wait until state is no longer LOADING
        while (entry->state.Get(MemoryOrder::SEQUENTIAL) == ShaderMapEntry::State::LOADING)
        {
            // sanity check - should never happen
            Assert(entry->loadingThreadId != CurrentThreadId());

            if (numSpins == maxSpins)
            {
                HYP_LOG(Shader, Warning,
                    "Shader {} is loading for too long! Skipping reuse attempt and loading on this thread. (Loading thread: {}, current thread: {})",
                    definition.GetName(),
                    entry->loadingThreadId.GetName(),
                    CurrentThreadId().GetName());

                entry = MakeRefCountedPtr<ShaderMapEntry>();
                entry->state.Set(ShaderMapEntry::State::LOADING, MemoryOrder::SEQUENTIAL);
                entry->loadingThreadId = CurrentThreadId();

                shouldAddEntryToCache = false;

                break;
            }

            ThreadSleep(0);

            numSpins++;
        }

        if (ShaderRef shader = entry->shader.Lock())
        {
            if (ensureContainsProperties(definition.GetProperties(), shader->GetCompiledShader()->GetProperties()))
            {
                return shader;
            }
            else
            {
                HYP_LOG(Shader, Error,
                    "Loaded shader from cache (Name: {}, Properties: {}) does not contain the requested properties!\n\tRequested: {}",
                    definition.GetName(),
                    shader->GetCompiledShader()->GetProperties().ToString(),
                    definition.GetProperties().ToString());
            }
        }

        // @TODO: remove bad value from cache?
    }

    RC<CompiledShader> compiledShader = MakeRefCountedPtr<CompiledShader>();

    if (!entry)
    {
        entry = MakeRefCountedPtr<ShaderMapEntry>();
        entry->compiledShader = compiledShader;
        entry->state.Set(ShaderMapEntry::State::LOADING, MemoryOrder::SEQUENTIAL);
        entry->loadingThreadId = CurrentThreadId();
    }

    if (shouldAddEntryToCache)
    {
        AssertDebug(entry != nullptr);

        Mutex::Guard guard(m_mutex);

        // double check, as we don't want to overwrite an entry, potentially invalidating references to the compiled shader.
        auto it = m_map.Find(definition);

        if (it == m_map.End())
        {
            m_map.Insert(definition, entry);
        }
    }

    ShaderRef shader;

    { // loading / compilation of shader (outside of mutex lock)

        bool isValidCompiledShader = true;
        isValidCompiledShader &= g_shaderCompiler->GetCompiledShader(definition.GetName(), definition.GetProperties(), *compiledShader);
        isValidCompiledShader &= compiledShader->GetDefinition().IsValid();

        Assert(isValidCompiledShader, "Compiled shader '{}' with properties hash {} is not a valid compiled shader",
            definition.GetName().LookupString(),
            definition.GetProperties().GetHashCode().Value());

        shader = g_renderBackend->MakeShader(std::move(compiledShader));

#ifdef HYP_DEBUG_MODE
        Assert(ensureContainsProperties(definition.GetProperties(), shader->GetCompiledShader()->GetDefinition().GetProperties()));
#endif

        DeferCreate(shader);

        // Update the entry
        entry->shader = shader;
        entry->state.Set(ShaderMapEntry::State::LOADED, MemoryOrder::SEQUENTIAL);
    }

    return shader;
}

ShaderRef ShaderManager::GetOrCreate(Name name, const ShaderProperties& props)
{
    return GetOrCreate(ShaderDefinition { name, props });
}

SizeType ShaderManager::CalculateMemoryUsage() const
{
    HYP_SCOPE;

    SizeType totalMemoryUsage = 0;

    Mutex::Guard guard(m_mutex);

    for (const auto& it : m_map)
    {
        totalMemoryUsage += sizeof(it.first);
        totalMemoryUsage += sizeof(it.second);

        if (const RC<ShaderMapEntry>& entry = it.second)
        {
            if (ShaderRef shader = entry->shader.Lock())
            {
                for (const ByteBuffer& byteBuffer : shader->GetCompiledShader()->modules)
                {
                    totalMemoryUsage += byteBuffer.Size();
                }
            }
        }
    }

    return totalMemoryUsage;
}

} // namespace hyperion
