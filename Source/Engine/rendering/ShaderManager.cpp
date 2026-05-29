/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#include <RenderingPch.hpp>

#include <rendering/ShaderManager.hpp>
#include <rendering/ComputePipeline.hpp>
#include <rendering/RenderInterface.hpp>
#include <rendering/ShaderInstance.hpp>
#include <rendering/Shader.hpp>
#include <rendering/GraphicsPipelineCache.hpp>
#include <rendering/GenericPipelineCache.hpp>

#include <rendering/util/ShaderPropertyDictionary.hpp>

#include <Core/reflection/Handle.hpp>

#include <Core/threading/SharedMutex.hpp>
#include <Core/threading/ThreadSignal.hpp>

#include <Core/threading/util/ThreadId.hpp>

#include <engine/EngineStats.hpp>
#include <engine/EngineGlobals.hpp>

#include <engine/threads/RenderThread.hpp>

#if HYP_EDITOR
#include <editor/EditorTask.hpp>
#endif

#include <semaphore>

namespace Hyperion {

HYP_DECLARE_LOG_CHANNEL(Shader);

#if HYP_ENABLE_SHADER_RELOAD
HYP_DECLARE_LOG_CHANNEL(ShaderCompiler);
#endif

static EngineStatTimer s_statShaderCompilation { "Rendering/ShaderCompilation", /* resetPerFrame */ false };

static const Name s_nameFallbackShader = NAME("Fallback");

static ShaderCacheId GenerateShaderCacheId()
{
    static volatile int64 s_idCounter = 0;
    return ShaderCacheId(AtomicIncrement(&s_idCounter));
}

static constexpr HashCode GetShaderEntryHashCode(
    Name name,
    const ShaderPropertySet& propertySet,
    const VertexInputLayoutDesc& inputLayout)
{
    return name.GetHashCode()
        .Combine(propertySet.GetHashCode())
        .Combine(inputLayout.GetHashCode());
}

class ShaderManagerImpl
{
public:
    struct ShaderMapEntry
    {
        ShaderCacheId cacheId = InvalidShaderCacheId;
        ShaderInstanceRef shaderInstance;
        Shader* shader = nullptr;

        ThreadSignal threadSignal { false };

        bool IsLoading() const
        {
            return !threadSignal.IsSignalled();
        }

        bool IsLoaded() const
        {
            return threadSignal.IsSignalled()
                && shader != nullptr;
        }
    };

    struct CompileShaderRequest
    {
        Name shaderName;
        ShaderPropertySet properties;
        VertexInputLayoutDesc inputLayout;
        ShaderMapEntry* entry;

        ShaderInstanceRef shaderInstance;
    };

    TMap<HashCode, ShaderMapEntry*> m_entryMap;
    SharedMutex m_mutex;

    // these live forever to keep pointers valid
    SparsePagedArray<Shader, 16> m_compiledShaderCache;
    SparsePagedArray<ShaderMapEntry, 16> m_entries;

#if HYP_EDITOR
    EditorTaskScope m_editorTask;
#endif

    Mutex m_compilingShadersMutex; // mutex for tracking shaders we're compiling + editor task
    AtomicVar<uint32> m_numCompilingShaders = 0;
    TMap<String, Array<CompileShaderRequest*>> m_compilingShaders;
    std::binary_semaphore m_spActiveCompilationTask { 0 };

#if HYP_ENABLE_SHADER_RELOAD
    static constexpr uint32 ShaderReloadIntervalMs = 3000;
    AtomicVar<bool> m_shaderReloadShouldStop { false };
    Task<void> m_shaderReloadTask;

    volatile int32 m_isReloadingShaders = 0;
#endif

    ShaderManagerImpl()
    {
#if HYP_ENABLE_SHADER_RELOAD
        StartShaderReloadThread();
#endif
    }

    ~ShaderManagerImpl()
    {
#if HYP_ENABLE_SHADER_RELOAD
        StopShaderReloadThread();
#endif
    }

    static void CompileShader(CompileShaderRequest& request)
    {
        AssertDebug(!request.entry->threadSignal.IsSignalled());
        HYP_DEFER({ request.entry->threadSignal.Signal(); });

        bool isValid = true;
        isValid &= g_shaderCompiler->RequestShader(
            request.shaderName, request.properties, request.inputLayout, request.entry->shader);

        isValid &= request.entry->shader->IsValid();

        if (!isValid)
        {
            // use fallback shader on fail.

            if (request.shaderName != s_nameFallbackShader
                && g_shaderCompiler->IsGraphicsShaderBundle(request.shaderName))
            {
                HYP_LOG(Shader, Verbose,
                    "Failed to compile shader '{}', trying to load fallback...",
                    request.shaderName);

                // @TODO Show editor alert.

                static constexpr StringHash PropertiesToKeepForFallback[] = {
                    "SKINNING"_sh,
                    "INSTANCING"_sh
                };

                ShaderPropertySet fallbackProperties {};

                for (const ShaderPropertyId shaderPropertyId : request.properties.ToArray())
                {
                    ShaderProperty shaderProperty;
                    if (GetShaderPropertyById(shaderPropertyId, shaderProperty)
                        && std::find(std::begin(PropertiesToKeepForFallback), std::end(PropertiesToKeepForFallback), shaderProperty.name) != std::end(PropertiesToKeepForFallback))
                    {
                        fallbackProperties.Add(shaderPropertyId);
                    }
                }

                isValid = g_shaderCompiler->RequestShader(s_nameFallbackShader, fallbackProperties, request.inputLayout, request.entry->shader);

                isValid &= request.entry->shader->IsValid();

                Assert(isValid, "Shader compilation failed and fallback shader could not be loaded!");

                return;
            }

            HYP_LOG(Shader, Error, "Failed to compile shader '{}'", request.shaderName);

            Assert(false, "Compiled shader '{}' is not a valid compiled shader", request.shaderName);
        }

        ShaderInstanceRef si = RI.MakeShader(request.entry->shader);
        request.shaderInstance = si;

#if HYP_ENABLE_SHADER_RELOAD
        if (si.IsValid())
        {
            si->SetCompiledTimestamp(request.entry->shader->lastCompiledTimestamp);
        }
#endif

        CheckResult(si->Create());

        // Update the entry
        request.entry->shaderInstance = si;
    }

    void CompileShaders()
    {
        m_spActiveCompilationTask.acquire();

#if HYP_EDITOR
        UpdateEditorTask();
#endif

        TMap<String, Array<CompileShaderRequest*>> current;

        while (true)
        {
            {
                Mutex::Guard guard(m_compilingShadersMutex);

                if (m_compilingShaders.Empty())
                {
                    m_spActiveCompilationTask.release();

                    return;
                }

                current = std::move(m_compilingShaders);
            }

            for (const auto& it : current)
            {
                const Array<CompileShaderRequest*>& requests = it.second;

                for (CompileShaderRequest* request : requests)
                {
                    CompileShader(*request);

                    m_numCompilingShaders.Decrement(1, MemoryOrder::RELAXED);

#if HYP_EDITOR
                    UpdateEditorTask();
#endif
                }
            }

            ThreadSleep(10); // sleep to try and pick up more tasks before we finish
        }
    }

#if HYP_EDITOR
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

        if (m_numCompilingShaders.Get(MemoryOrder::ACQUIRE) == 0)
        {
            m_editorTask.Reset();
        }
        else
        {
            Mutex::Guard guard(m_compilingShadersMutex);

            if (!m_editorTask.GetEditorTask())
            {
                m_editorTask = EditorTaskScope(
                    TickableEditorTask::StaticClass(),
                    []()
                    { /* no tick function */ },
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
            const CompileShaderRequest& request,
            const ProcRef<void()>& taskFunction = nullptr,
            bool useTask = true)
            : impl(impl),
              request(request)
        {
            Assert(request.entry != nullptr);

            {
                Mutex::Guard guard(impl->m_compilingShadersMutex);

                const String shaderNameStr = *request.shaderName;

                impl->m_numCompilingShaders.Increment(1, MemoryOrder::RELAXED);
                impl->m_compilingShaders[shaderNameStr].PushBack(&this->request);
            }

            if (taskFunction)
            {
                if (useTask)
                {
                    task = TaskSystem::GetInstance().Enqueue([&taskFunction]
                        {
                            taskFunction();
                        },
                        TaskThreadPoolName::THREAD_POOL_BACKGROUND);
                }
                else
                {
                    taskFunction();
                }
            }
            else
            {
                impl->m_spActiveCompilationTask.release();

                if (useTask)
                {
                    task = TaskSystem::GetInstance().Enqueue([impl]
                        {
                            impl->CompileShaders();
                        },
                        TaskThreadPoolName::THREAD_POOL_BACKGROUND);
                }
                else
                {
                    impl->CompileShaders();
                }
            }
        }

        ~CompilingShaderScope()
        {
            Wait();
        }

        void Wait()
        {
            ENGINE_STAT_SCOPE(&s_statShaderCompilation);

            if (task.IsValid())
            {
                task.Await();
            }
        }
    };

    ShaderInstanceRef GetOrCreate(
        Name name, const ShaderPropertySet& properties, const VertexInputLayoutDesc& inputLayout,
        ShaderCacheId& outCacheId, bool doLoadShader)
    {
        HYP_NAMED_SCOPE("Get shader from cache or create");

        const HashCode hc = GetShaderEntryHashCode(name, properties, inputLayout);

        const auto EnsureMatch = [](const ShaderPropertySet& expectedProperties, const VertexInputLayoutDesc& expectedInputLayout, const Shader& received) -> bool
        {
            if (received.inputLayout.mask != expectedInputLayout.mask)
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
            while (entry->IsLoading())
            {
                entry->threadSignal.Wait();
            }

            Assert(entry->shaderInstance.IsValid());

            if (true)
            // if (EnsureMatch(properties, inputLayout, *entry->shaderInstance->GetShader()))
            {
                return entry->shaderInstance;
            }
            else
            {
                HYP_LOG(Shader, Error, "Loaded shader from cache (Name: {}) does not contain the requested properties!", name);
            }
        }

        if (!entry)
        {
            ShaderCacheId cacheId = GenerateShaderCacheId();

            entry = GetShaderMapEntry(cacheId);

            Assert(!entry->IsLoaded());

            entry->cacheId = cacheId;
            entry->shader = GetShader(entry->cacheId);

            entry->threadSignal.Signal();
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

        CompileShaderRequest request {};
        request.shaderName = name;
        request.properties = properties;
        request.inputLayout = inputLayout;
        request.entry = entry;

        CompilingShaderScope compilingShaderScope { this, request };
        compilingShaderScope.Wait();

        return entry->shaderInstance;
    }

    ShaderCacheId GetShaderCacheId(
        Name name,
        const ShaderPropertySet& properties,
        const VertexInputLayoutDesc& inputLayout,
        bool createIfNotExists = false)
    {
        const HashCode hc = GetShaderEntryHashCode(name, properties, inputLayout);

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
            (void)GetOrCreate(name, properties, inputLayout, cacheId, /* doLoadShader */ false);
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

    size_t CalculateMemoryUsage() const
    {
        HYP_SCOPE;

        size_t totalMemoryUsage = 0;

        TSharedLock lock(m_mutex);

        for (const auto& it : m_entryMap)
        {
            totalMemoryUsage += sizeof(it.first);
            totalMemoryUsage += sizeof(it.second);

            if (const ShaderMapEntry* entry = it.second)
            {
                for (const BlobDataReference& blob : entry->shaderInstance->GetShader()->shaderBlobs)
                {
                    totalMemoryUsage += blob.size;
                }
            }
        }

        return totalMemoryUsage;
    }

#if HYP_ENABLE_SHADER_RELOAD
    void StartShaderReloadThread()
    {
        m_shaderReloadShouldStop.Set(false, MemoryOrder::RELAXED);

        m_shaderReloadTask = TaskSystem::GetInstance().Enqueue(
            [this]()
            {
                auto CheckShouldStop = [&]()
                {
                    return m_shaderReloadShouldStop.Get(MemoryOrder::RELAXED)
                        || static_cast<TaskThread*>(CurrentThreadObject())->IsStopping();
                };

                while (!CheckShouldStop())
                {
                    ThreadSleep(ShaderReloadIntervalMs);

                    if (CheckShouldStop())
                    {
                        break;
                    }

                    RecheckLoadedShaders();
                }
            },
            TaskThreadPoolName::THREAD_POOL_BACKGROUND);
    }

    void StopShaderReloadThread()
    {
        m_shaderReloadShouldStop.Set(true, MemoryOrder::RELAXED);

        if (m_shaderReloadTask.IsValid())
        {
            m_shaderReloadTask.Await();
        }
    }

    void RecheckLoadedShaders()
    {
        {
            int32 expected = 0;
            if (!AtomicCompareExchange(&m_isReloadingShaders, expected, 1))
            {
                return;
            }
        }

        Array<CompileShaderRequest> requests;

        {
            TSharedLock lock(m_mutex);

            for (const auto& it : m_entryMap)
            {
                ShaderMapEntry* entry = it.second;

                if (!entry || !entry->IsLoaded())
                {
                    continue;
                }

                if (!entry->shader)
                {
                    continue;
                }

                if (!g_shaderCompiler->IsShaderBundleOutdated(
                        entry->shader->baseName,
                        entry->shader->lastCompiledTimestamp))
                {
                    continue;
                }

                CompileShaderRequest& request = requests.EmplaceBack();
                request.shaderName = entry->shader->baseName;
                request.properties = entry->shader->properties;
                request.inputLayout = entry->shader->inputLayout;
                request.entry = entry;
            }
        }

        if (requests.Empty())
        {
            AtomicExchange(&m_isReloadingShaders, 0);

            return;
        }

#if 0 // HYP_EDITOR
        {
            Array<String> reloadingShaderNames;
            reloadingShaderNames.Reserve(staleEntries.Size());

            for (ShaderMapEntry* entry : staleEntries)
            {
                reloadingShaderNames.PushBack(*entry->shader->baseName);
            }

            const String descText = String::Join(reloadingShaderNames, "\n");

            if (!m_editorTask.GetEditorTask())
            {
                m_editorTask = EditorTaskScope(
                    TickableEditorTask::StaticClass(),
                    []()
                    { /* no tick function */ },
                    "Reloading shaders",
                    descText,
                    /* isForegroundTask */ true);
            }
            else
            {
                m_editorTask.GetEditorTask()->SetDescription(descText);
            }
        }
#endif

        HYP_LOG(ShaderCompiler, Info, "Reloading {} shaders...", requests.Size());

        TSet<Shader*> shadersToExpire;

        for (CompileShaderRequest& request : requests)
        {
            if (!request.entry->IsLoaded())
            {
                continue;
            }

            request.entry->threadSignal.Reset();

            CompilingShaderScope compilingShaderScope(
                this,
                request,
                nullptr,
                /* useTask */ false);

            compilingShaderScope.Wait();

            Assert(request.entry->shader != nullptr);

            shadersToExpire.Add(request.entry->shader);

            AssertDebug(request.entry->IsLoaded());
        }

        if (shadersToExpire.Any())
        {
            for (Shader* shader : shadersToExpire)
            {
                RI.graphicsPipelineCache->ExpirePipelinesForShader(shader);
                RI.computePipelineCache->ExpirePipelinesForShader(shader);
                RI.rayTracingPipelineCache->ExpirePipelinesForShader(shader);
            }
        }

        AtomicExchange(&m_isReloadingShaders, 0);
    }
#endif // HYP_ENABLE_SHADER_RELOAD
};

ShaderManager::ShaderManager()
    : m_impl(MakePimplWithAllocator<ShaderManagerImpl, RenderAllocator>())
{
}

ShaderInstanceRef ShaderManager::GetOrCreate(Name name, const ShaderPropertySet& propertySet, const VertexInputLayoutDesc& inputLayout)
{
    AssertDebug(name.IsValid());

    ShaderCacheId cacheId;
    return m_impl->GetOrCreate(name, propertySet, inputLayout, cacheId, /* doLoadShader */ true);
}

ShaderCacheId ShaderManager::GetShaderCacheId(
    Name name,
    const ShaderPropertySet& properties,
    const VertexInputLayoutDesc& inputLayout,
    bool createIfNotExists) const
{
    return m_impl->GetShaderCacheId(name, properties, inputLayout, createIfNotExists);
}

size_t ShaderManager::CalculateMemoryUsage() const
{
    return m_impl->CalculateMemoryUsage();
}

} // namespace Hyperion
