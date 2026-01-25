/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <RenderingPch.hpp>

#include <rendering/ShaderManager.hpp>
#include <rendering/ComputePipeline.hpp>
#include <rendering/RenderInterface.hpp>
#include <rendering/Shader.hpp>

#include <core/reflection/Handle.hpp>

#include <core/threading/SharedMutex.hpp>

namespace Hyperion {

static ShaderCacheId GenerateShaderCacheId()
{
    static volatile int64 s_idCounter = 0;
    return ShaderCacheId(AtomicIncrement(&s_idCounter));
}

class ShaderManagerImpl
{
public:
    struct ShaderMapEntry
    {
        enum class State : uint8
        {
            UNLOADED = 0,
            LOADING = 1,
            LOADED = 2
        };

        ShaderCacheId cacheId;
        ShaderRef shaderInstance;
        CompiledShader* compiledShader = nullptr;
        AtomicVar<State> state = State::UNLOADED;
        ThreadId loadingThreadId;
    };

    HashMap<ShaderDefinition, ShaderMapEntry*> m_entryMap;
    SharedMutex m_mutex;

    // these live forever to keep pointers valid
    SparsePagedArray<CompiledShader, 16> m_compiledShaderCache;
    SparsePagedArray<ShaderMapEntry, 16> m_entries;

    ShaderRef GetOrCreate(const ShaderDefinition& definition, ShaderCacheId& outCacheId, bool doLoadShader)
    {
        HYP_NAMED_SCOPE("Get shader from cache or create");

        const auto EnsureContainsProperties = [](const ShaderProperties& expected, const ShaderProperties& received) -> bool
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

        outCacheId = InvalidShaderCacheId;

        ShaderMapEntry* entry = nullptr;
        bool shouldAddEntryToCache = false;

        { // pulling value from cache if it exists
            TSharedLock lock(m_mutex);

            auto it = m_entryMap.Find(definition);

            if (it != m_entryMap.End())
            {
                entry = it->second;
            }
            else
            {
                shouldAddEntryToCache = true;
            }
        }

        if (entry != nullptr && doLoadShader)
        {
            constexpr int MaxSpins = 1024;
            int numSpins = 0;

            // loading from another thread -- wait until state is no longer LOADING
            while (entry->state.Get(MemoryOrder::SEQUENTIAL) == ShaderMapEntry::State::LOADING)
            {
                // sanity check - should never happen
                Assert(entry->loadingThreadId != CurrentThreadId());

                if (numSpins == MaxSpins)
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

            if (EnsureContainsProperties(definition.GetProperties(), entry->shaderInstance->GetCompiledShader()->GetProperties()))
            {
                return entry->shaderInstance;
            }
            else
            {
                HYP_LOG(Shader, Error,
                    "Loaded shader from cache (Name: {}, Properties: {}) does not contain the requested properties!\n\tRequested: {}",
                    definition.GetName(),
                    entry->shaderInstance->GetCompiledShader()->GetProperties().ToString(),
                    definition.GetProperties().ToString());
            }
        }

        if (!entry)
        {
            ShaderCacheId cacheId = GenerateShaderCacheId();

            entry = GetShaderMapEntry(cacheId);
            entry->cacheId = cacheId;
            entry->compiledShader = GetCompiledShader(entry->cacheId);

            if (doLoadShader)
            {
                entry->state.Set(ShaderMapEntry::State::LOADING, MemoryOrder::SEQUENTIAL);
                entry->loadingThreadId = CurrentThreadId();
            }
        }

        outCacheId = entry->cacheId;

        if (shouldAddEntryToCache)
        {
            AssertDebug(entry != nullptr);

            TUniqueLock lock(m_mutex);

            // double check, as we don't want to overwrite an entry, potentially invalidating references to the compiled shader.
            auto it = m_entryMap.Find(definition);

            if (it == m_entryMap.End())
            {
                m_entryMap.Insert(definition, entry);
            }
        }

        if (!doLoadShader)
        {
            return ShaderRef::Null();
        }

        ShaderRef shader;

        { // loading / compilation of shader (outside of mutex lock)

            bool isValidCompiledShader = true;
            isValidCompiledShader &= g_shaderCompiler->GetCompiledShader(definition.GetName(), definition.GetProperties(), *entry->compiledShader);
            isValidCompiledShader &= entry->compiledShader->GetDefinition().IsValid();

            Assert(isValidCompiledShader, "Compiled shader '{}' with properties hash {} is not a valid compiled shader",
                definition.GetName().LookupString(),
                definition.GetProperties().GetHashCode().Value());

            shader = g_renderInterface->MakeShader(entry->compiledShader);

#ifdef HYP_DEBUG_MODE
            Assert(EnsureContainsProperties(definition.GetProperties(), shader->GetCompiledShader()->GetDefinition().GetProperties()));
#endif

            DeferCreate(shader);

            // Update the entry
            entry->shaderInstance = shader;
            entry->state.Set(ShaderMapEntry::State::LOADED, MemoryOrder::SEQUENTIAL);
        }

        return shader;
    }

    ShaderRef GetOrCreate(Name name, const ShaderProperties& props)
    {
        ShaderCacheId cacheIdUnused;
        return GetOrCreate(ShaderDefinition { name, props }, cacheIdUnused, /* doLoadShader */ true);
    }

    ShaderCacheId GetShaderCacheId(const ShaderDefinition& definition, bool createIfNotExists = false)
    {
        { // check through mapping
            TSharedLock lock(m_mutex);

            auto it = m_entryMap.Find(definition);

            if (it != m_entryMap.End())
            {
                return it->second->cacheId;
            }
        }

        ShaderCacheId cacheId = InvalidShaderCacheId;

        if (createIfNotExists)
        {
            // create the shader entry - don't request loading the shader though.
            (void)GetOrCreate(definition, cacheId, /* doLoadShader */ false);
        }

        return cacheId;
    }

    const ShaderDefinition* GetShaderDefinition(ShaderCacheId shaderCacheId) const
    {
        TSharedLock lock(m_mutex);

        const ShaderMapEntry* entry = m_entries.TryGet(uint64(shaderCacheId));

        if (!entry)
        {
            return nullptr;
        }

        return &entry->compiledShader->definition;
    }

    CompiledShader* GetCompiledShader(ShaderCacheId shaderCacheId)
    {
        TSharedLock lock(m_mutex);

        if (!m_compiledShaderCache.HasIndex(uint64(shaderCacheId)))
        {
            lock.Reset();

            TUniqueLock uniqueLock(m_mutex);

            if (!m_compiledShaderCache.HasIndex(uint64(shaderCacheId)))
            {
                return &*m_compiledShaderCache.Emplace(uint64(shaderCacheId));
            }

            // someone else added it before we did
            lock.Reset(m_mutex);
        }
        
        return &m_compiledShaderCache.Get(uint64(shaderCacheId));
    }

    ShaderMapEntry* GetShaderMapEntry(ShaderCacheId shaderCacheId)
    {
        TSharedLock lock(m_mutex);

        if (!m_entries.HasIndex(uint64(shaderCacheId)))
        {
            lock.Reset();

            TUniqueLock uniqueLock(m_mutex);

            if (!m_entries.HasIndex(uint64(shaderCacheId)))
            {
                return &*m_entries.Emplace(uint64(shaderCacheId));
            }

            // someone else added it before we did
            lock.Reset(m_mutex);
        }
        
        return &m_entries.Get(uint64(shaderCacheId));
    }

    SizeType CalculateMemoryUsage() const
    {
        HYP_SCOPE;

        SizeType totalMemoryUsage = 0;

        TSharedLock lock(m_mutex);

        for (const auto& it : m_entryMap)
        {
            totalMemoryUsage += sizeof(it.first);
            totalMemoryUsage += sizeof(it.second);

            if (const ShaderMapEntry* entry = it.second)
            {
                for (const ByteBuffer& byteBuffer : entry->shaderInstance->GetCompiledShader()->shaderBlobs)
                {
                    totalMemoryUsage += byteBuffer.Size();
                }
            }
        }

        return totalMemoryUsage;
    }
};

ShaderManager* ShaderManager::GetInstance()
{
    return g_shaderManager;
}

ShaderManager::ShaderManager()
    : m_impl(MakePimpl<ShaderManagerImpl>())
{
}

ShaderRef ShaderManager::GetOrCreate(Name name, const ShaderProperties& props)
{
    return m_impl->GetOrCreate(name, props);
}

ShaderRef ShaderManager::GetOrCreate(const ShaderDefinition& definition)
{
    ShaderCacheId cacheIdUnused;
    return m_impl->GetOrCreate(definition, cacheIdUnused, /* doLoadShader */ true);
}

ShaderRef ShaderManager::GetOrCreate(const ShaderDefinition& definition, ShaderCacheId& outCacheId)
{
    return m_impl->GetOrCreate(definition, outCacheId, /* doLoadShader */ true);
}

ShaderCacheId ShaderManager::GetShaderCacheId(const ShaderDefinition& definition, bool createIfNotExists) const
{
    return m_impl->GetShaderCacheId(definition, createIfNotExists);
}

const ShaderDefinition* ShaderManager::GetShaderDefinition(ShaderCacheId shaderCacheId) const
{
    if (shaderCacheId == InvalidShaderCacheId)
    {
        return nullptr;
    }

    return m_impl->GetShaderDefinition(shaderCacheId);
}

SizeType ShaderManager::CalculateMemoryUsage() const
{
    return m_impl->CalculateMemoryUsage();
}

} // namespace Hyperion
