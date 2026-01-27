/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <RenderingPch.hpp>

#include <rendering/ShaderManager.hpp>
#include <rendering/ComputePipeline.hpp>
#include <rendering/RenderInterface.hpp>
#include <rendering/Shader.hpp>

#include <rendering/util/ShaderPropertyCache.hpp>

#include <core/reflection/Handle.hpp>

#include <core/threading/SharedMutex.hpp>

namespace Hyperion {

static ShaderCacheId GenerateShaderCacheId()
{
    static volatile int64 s_idCounter = 0;
    return ShaderCacheId(AtomicIncrement(&s_idCounter));
}

static constexpr HashCode GetShaderEntryHashCode(
    Name name,
    const ShaderPropertySet& propertySet,
    const VertexAttributeSet& vertexAttributes)
{
    return name.GetHashCode()
        .Combine(propertySet.GetHashCode())
        .Combine(vertexAttributes.GetHashCode());
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

    HashMap<HashCode, ShaderMapEntry*> m_entryMap;
    SharedMutex m_mutex;

    // these live forever to keep pointers valid
    SparsePagedArray<CompiledShader, 16> m_compiledShaderCache;
    SparsePagedArray<ShaderMapEntry, 16> m_entries;

    ShaderRef GetOrCreate(
        Name name, const ShaderPropertySet& properties, const VertexAttributeSet& vertexAttributes,
        ShaderCacheId& outCacheId, bool doLoadShader)
    {
        HYP_NAMED_SCOPE("Get shader from cache or create");

        const HashCode hc = GetShaderEntryHashCode(name, properties, vertexAttributes);

        const auto EnsureMatch = [](
            const ShaderPropertySet& expectedProperties, const VertexAttributeSet& expectedVertexAttributes,
            const CompiledShader& received) -> bool
        {
            if (received.vertexAttributes != expectedVertexAttributes)
            {
                return false;
            }

            uint64 chunkOffset = 0;
            for (uint64 chunk : expectedProperties.chunks)
            {
                FOR_EACH_BIT(chunk, bit)
                {
                    ShaderPropertyId propertyId = ShaderPropertyId(chunkOffset + bit);
                    
                    if (!received.properties.Test(propertyId))
                    {
                        return false;
                    }
                }

                chunkOffset += 64;
            }

            return true;
        };

        outCacheId = InvalidShaderCacheId;

        ShaderMapEntry* entry = nullptr;
        bool shouldAddEntryToCache = false;

        { // pulling value from cache if it exists
            TSharedLock lock(m_mutex);

            auto it = m_entryMap.Find(hc);

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
                        name,
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

            if (EnsureMatch(
                properties, vertexAttributes,
                *entry->shaderInstance->GetCompiledShader()))
            {
                return entry->shaderInstance;
            }
            else
            {
                HYP_LOG(Shader, Error,
                    "Loaded shader from cache (Name: {}) does not contain the requested properties!",
                    name);
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
            auto it = m_entryMap.Find(hc);

            if (it == m_entryMap.End())
            {
                m_entryMap.Insert(hc, entry);
            }
        }

        if (!doLoadShader)
        {
            return ShaderRef::Null();
        }

        ShaderRef shader;

        { // loading / compilation of shader (outside of mutex lock)
            bool isValidCompiledShader = true;
            isValidCompiledShader &= g_shaderCompiler->GetCompiledShader(name, properties, vertexAttributes, *entry->compiledShader);
            isValidCompiledShader &= entry->compiledShader->IsValid();

            Assert(isValidCompiledShader, "Compiled shader '{}' is not a valid compiled shader", name);

            shader = g_renderInterface->MakeShader(entry->compiledShader);

#ifdef HYP_DEBUG_MODE
            Assert(EnsureMatch(properties, vertexAttributes, *shader->GetCompiledShader()));
#endif

            DeferCreate(shader);

            // Update the entry
            entry->shaderInstance = shader;
            entry->state.Set(ShaderMapEntry::State::LOADED, MemoryOrder::SEQUENTIAL);
        }

        return shader;
    }

    ShaderCacheId GetShaderCacheId(
        Name name,
        const ShaderPropertySet& properties,
        const VertexAttributeSet& vertexAttributes,
        bool createIfNotExists = false)
    {
        const HashCode hc = GetShaderEntryHashCode(name, properties, vertexAttributes);

        { // check through mapping
            TSharedLock lock(m_mutex);

            auto it = m_entryMap.Find(hc);

            if (it != m_entryMap.End())
            {
                return it->second->cacheId;
            }
        }

        ShaderCacheId cacheId = InvalidShaderCacheId;

        if (createIfNotExists)
        {
            // create the shader entry - don't request loading the shader though.
            (void)GetOrCreate(name, properties, vertexAttributes, cacheId, /* doLoadShader */ false);
        }

        return cacheId;
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

ShaderRef ShaderManager::GetOrCreate(Name name, const ShaderPropertySet& propertySet, const VertexAttributeSet& vertexAttributes)
{
    ShaderCacheId cacheId;
    return m_impl->GetOrCreate(name, propertySet, vertexAttributes, cacheId, /* doLoadShader */ true);
}

ShaderCacheId ShaderManager::GetShaderCacheId(
    Name name,
    const ShaderPropertySet& properties,
    const VertexAttributeSet& vertexAttributes,
    bool createIfNotExists) const
{
    return m_impl->GetShaderCacheId(name, properties, vertexAttributes, createIfNotExists);
}

SizeType ShaderManager::CalculateMemoryUsage() const
{
    return m_impl->CalculateMemoryUsage();
}

} // namespace Hyperion
