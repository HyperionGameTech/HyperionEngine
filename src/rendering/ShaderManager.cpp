/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <RenderingPch.hpp>

#include <rendering/ShaderManager.hpp>
#include <rendering/ComputePipeline.hpp>
#include <rendering/RenderInterface.hpp>
#include <rendering/Shader.hpp>

#include <rendering/util/ShaderPropertyDictionary.hpp>

#include <core/reflection/Handle.hpp>

#include <core/threading/SharedMutex.hpp>

#ifdef HYP_EDITOR
#include <editor/EditorTask.hpp>
#endif

#include <semaphore>

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

        ShaderCacheId cacheId = InvalidShaderCacheId;
        ShaderInstanceRef shaderInstance;
        Shader* shader = nullptr;
        AtomicVar<State> state = State::UNLOADED;
        ThreadId loadingThreadId;
    };

    struct CompileShaderRequest
    {
        Name shaderName;
        ShaderPropertySet properties;
        VertexAttributeSet attributes;
        ShaderMapEntry* entry;

        ShaderInstanceRef outShader;
    };

    HashMap<HashCode, ShaderMapEntry*> m_entryMap;
    SharedMutex m_mutex;

    // these live forever to keep pointers valid
    SparsePagedArray<Shader, 16> m_compiledShaderCache;
    SparsePagedArray<ShaderMapEntry, 16> m_entries;

#if HYP_EDITOR
    EditorTaskScope m_editorTask;
#endif

    Mutex m_compilingShadersMutex; // mutex for tracking shaders we're compiling + editor task
    uint32 m_numCompilingShaders = 0;
    HashMap<String, Array<CompileShaderRequest*>> m_compilingShaders;
    std::binary_semaphore m_activeCompilationTask { 0 };

    static void CompileShader(CompileShaderRequest& request)
    {
        bool isValid = true;
        isValid &= g_shaderCompiler->RequestShader(
            request.shaderName, request.properties, request.attributes, *request.entry->shader);

        isValid &= request.entry->shader->IsValid();

        Assert(isValid, "Compiled shader '{}' is not a valid compiled shader", request.shaderName);

        request.outShader = g_renderInterface->MakeShader(request.entry->shader);
        CheckResult(request.outShader->Create());

        // Update the entry
        request.entry->shaderInstance = request.outShader;
        request.entry->state.Set(ShaderMapEntry::State::LOADED, MemoryOrder::SEQUENTIAL);
    }

    void CompileShaders()
    {
        m_activeCompilationTask.acquire();

#ifdef HYP_EDITOR
        UpdateEditorTask();
#endif

        HashMap<String, Array<CompileShaderRequest*>> current;

        while (true)
        {
            {
                Mutex::Guard guard(m_compilingShadersMutex);

                if (m_compilingShaders.Empty())
                {
                    m_activeCompilationTask.release();

                    return;
                }

                current = m_compilingShaders;
            }

            for (const auto& it : current)
            {
                const String& name = it.first;
                const Array<CompileShaderRequest*>& requests = it.second;

                for (CompileShaderRequest* request : requests)
                {
                    CompileShader(*request);

                    Mutex::Guard guard(m_compilingShadersMutex);

                    auto compilingShadersIt = m_compilingShaders.Find(name);
                    Assert(compilingShadersIt != m_compilingShaders.End());

                    compilingShadersIt->second.Erase(request);

                    if (compilingShadersIt->second.Empty())
                    {
                        m_compilingShaders.Erase(name);
                    }

                    --m_numCompilingShaders;

#ifdef HYP_EDITOR
                    UpdateEditorTask();
#endif
                }
            }

            ThreadSleep(100); // sleep to try and pick up more tasks before we finish
        }
    }
    
#ifdef HYP_EDITOR
    void UpdateEditorTask()
    {
        auto GetDescriptionText = [this]()
        {
            Array<String> shaderNames;
            shaderNames.Reserve(m_compilingShaders.Size());

            for (const auto& pair : m_compilingShaders)
            {
                if (pair.second.Empty())
                    continue;

                shaderNames.PushBack(HYP_FORMAT("{} ({})", pair.first, pair.second.Size()));
            }

            return String::Join(shaderNames, "\n");
        };
            
        if (m_numCompilingShaders == 0)
        {
            m_editorTask.Reset();
        }
        else
        {
            if (!m_editorTask.GetEditorTask())
            {
                m_editorTask = EditorTaskScope(
                    TickableEditorTask::StaticClass(),
                    []() { /* no tick function */ },
                    "Preparing shaders",
                    GetDescriptionText(),
                    /* isForegroundTask */ true);
            }
            else
            {
                m_editorTask.GetEditorTask()->SetDescription(GetDescriptionText());
            }
        }
    }
#endif

    // scope to inc/dec atomic counter for number of shaders actively being compiled
    // so we can display a toast to let the user know what's going on in editor
    struct CompilingShaderScope
    {
        ShaderManagerImpl* impl;
        CompileShaderRequest request;
        Task<void> task;

        CompilingShaderScope(
            ShaderManagerImpl* impl,
            Name shaderName,
            const ShaderPropertySet& properties,
            const VertexAttributeSet& attributes,
            ShaderMapEntry* entry)
            : impl(impl)
        {
            Assert(entry != nullptr);

            request = {};
            request.shaderName = shaderName;
            request.properties = properties;
            request.attributes = attributes;
            request.entry = entry;
            
            {
                Mutex::Guard guard(impl->m_compilingShadersMutex);

                const String shaderNameStr = *shaderName;

                impl->m_numCompilingShaders++;
                impl->m_compilingShaders[shaderNameStr].PushBack(&request);

                impl->m_activeCompilationTask.release();
            }
            
            task = TaskSystem::GetInstance().Enqueue([impl, req = &request]()
                {
                    impl->CompileShaders();
                },
                TaskThreadPoolName::THREAD_POOL_BACKGROUND);
        }

        ~CompilingShaderScope()
        {
            Wait();
        }

        void Wait()
        {
            if (task.IsValid())
            {
                task.Await();
            }
        }

    };

    ShaderInstanceRef GetOrCreate(
        Name name, const ShaderPropertySet& properties, const VertexAttributeSet& vertexAttributes,
        ShaderCacheId& outCacheId, bool doLoadShader)
    {
        HYP_NAMED_SCOPE("Get shader from cache or create");

        const HashCode hc = GetShaderEntryHashCode(name, properties, vertexAttributes);

        const auto EnsureMatch = [](
            const ShaderPropertySet& expectedProperties, const VertexAttributeSet& expectedVertexAttributes,
            const Shader& received) -> bool
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
                *entry->shaderInstance->GetShader()))
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
            entry->shader = GetShader(entry->cacheId);

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
            return ShaderInstanceRef::Null();
        }

        CompilingShaderScope compilingShaderScope(this, name, properties, vertexAttributes, entry);
        compilingShaderScope.Wait();

        return entry->shaderInstance;
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

    Shader* GetShader(ShaderCacheId shaderCacheId)
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
                for (const ByteBuffer& byteBuffer : entry->shaderInstance->GetShader()->shaderBlobs)
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

ShaderInstanceRef ShaderManager::GetOrCreate(Name name, const ShaderPropertySet& propertySet, const VertexAttributeSet& vertexAttributes)
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
