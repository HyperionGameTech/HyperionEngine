/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#include <RenderingPch.hpp>

#include <Rendering/ShaderManager.hpp>
#include <Rendering/ComputePipeline.hpp>
#include <Rendering/RenderInterface.hpp>
#include <Rendering/ShaderInstance.hpp>
#include <Rendering/Shader.hpp>
#include <Rendering/GraphicsPipelineCache.hpp>
#include <Rendering/GenericPipelineCache.hpp>

#include <Rendering/Util/ShaderCompiler.hpp>
#include <Rendering/Util/ShaderPropertyDictionary.hpp>

#include <Asset/AssetRegistry.hpp>

#include <Core/Reflection/Handle.hpp>

#include <Core/Containers/SparsePagedArray.hpp>

#include <Core/IO/ByteWriter.hpp>

#include <Core/Threading/SharedMutex.hpp>
#include <Core/Threading/Guarded.hpp>

#include <Core/Threading/Util/ThreadId.hpp>

#include <Framework/EngineStats.hpp>
#include <Framework/EngineGlobals.hpp>

#include <Framework/Threads/RenderThread.hpp>

#ifdef HYP_EDITOR
#include <Editor/EditorTask.hpp>
#endif // HYP_EDITOR

namespace Hyperion {

ENGINE_API HYP_DECLARE_LOG_CHANNEL(Shader);

// iOS can't save the file
#if !defined(HYP_IOS)
#define HYP_GENERATE_SHADER_PRELOAD_CACHE
#endif

static EngineStatTimer s_statShaderCompilation("TotalShaderCompilationTime", /* resetPerFrame */ false);

static const Name s_nameFallbackShader = NAME("Fallback");

static Pool s_shaderPool { 1 * 1024 * 1024, PF_THREAD_SAFE };
Pool* g_shaderPool = &s_shaderPool;

using ShaderAllocator = AllocatorInstance<Pool, &g_shaderPool>;

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

        Task<ShaderInstanceRef> compileTask;

        bool IsLoading() const
        {
            return !compileTask.IsValid() || !compileTask.IsCompleted();
        }

        bool IsLoaded() const
        {
            return compileTask.IsValid() && compileTask.IsCompleted();
        }
    };

    using EntryMap = Map<HashCode, ShaderMapEntry*, ShaderAllocator, HashTablePolicy::NotPooled>;

    EntryMap m_entryMap;
    SharedMutex m_mutex;

    // these live forever to keep pointers valid
    SparsePagedArray<Shader, 16, ShaderAllocator> m_compiledShaderCache;
    SparsePagedArray<ShaderMapEntry, 16, ShaderAllocator> m_entries;

    AtomicVar<uint32> m_numCompilingShaders = 0;
    AtomicVar<uint32> m_totalNumCompilingShadersForTask = 0;

#ifdef HYP_EDITOR
    Guarded<EditorTaskScope> m_editorTask;

    Mutex m_compilingShadersMutex;
    Map<Name, uint32, ShaderAllocator, HashTablePolicy::NotPooled> m_compilingShaderNames;
#endif

#if HYP_ENABLE_SHADER_RELOAD
    static constexpr uint32 ShaderReloadIntervalMs = 3000;
    AtomicVar<bool> m_shaderReloadShouldStop { false };
    Task<void> m_shaderReloadTask;

    volatile int32 m_isReloadingShaders = 0;
#endif

    Mutex m_cacheWriteMutex;

#ifdef HYP_GENERATE_SHADER_PRELOAD_CACHE
    Array<ShaderPreloadEntry, ShaderAllocator> m_shaderPreloadEntriesToWrite;
    Set<HashCode, ShaderAllocator> m_shaderPreloadEntriesToWriteHashes;
#endif

    Array<ShaderPreloadEntry, ShaderAllocator> m_preloadCacheEntries;

    ShaderManagerImpl()
    {
#if HYP_ENABLE_SHADER_RELOAD
        StartShaderReloadThread();
#endif

        // Try finding preload bin
        FileByteReader preloadCacheReader(EngineGlobals::GetCacheDirectory() / "shaderpreload.bin");

        if (!preloadCacheReader.Eof())
        {
            // Calculate total count
            if (preloadCacheReader.Max() % sizeof(ShaderPreloadEntry) == 0)
            {
                const size_t numEntries = preloadCacheReader.Max() / sizeof(ShaderPreloadEntry);
                m_preloadCacheEntries.Resize(numEntries);

                preloadCacheReader.Read(m_preloadCacheEntries.Data(), preloadCacheReader.Max());

                // If HYP_GENERATE_SHADER_PRELOAD_CACHE is defined; add all loaded entries
                //  into the write queue so we don't end up removing them from the cache entries
                //  on the next time we save

#ifdef HYP_GENERATE_SHADER_PRELOAD_CACHE
                m_shaderPreloadEntriesToWrite.Reserve(m_preloadCacheEntries.Size());

                for (size_t i = 0; i < m_preloadCacheEntries.Size(); i++)
                {
                    const ShaderPreloadEntry& entry = m_preloadCacheEntries[i];

                    const HashCode hashCode = HashCode::GetHashCode(
                        reinterpret_cast<const ubyte*>(&entry),
                        reinterpret_cast<const ubyte*>(&entry) + sizeof(ShaderPreloadEntry));

                    if (m_shaderPreloadEntriesToWriteHashes.Insert(hashCode).second)
                    {
                        m_shaderPreloadEntriesToWrite.PushBack(entry);
                    }
                }
#endif
            }
            else
            {
                HYP_LOG(Shader, Warning, "Shader preload cache is not a multiple of the preload entry struct size - skipping preload to prevent corruption issues, and removing the file");

                if (!preloadCacheReader.GetFilepath().Remove())
                {
                    HYP_LOG(Shader, Warning, "Shader preload cache file at {} could not be removed; ignoring", preloadCacheReader.GetFilepath());
                }
            }
        }
        else
        {
            // Not found; just continue anyway - next run will save em
            HYP_LOG(Shader, Info, "No shaderpreload.bin found in cache directory ({})", preloadCacheReader.GetFilepath());
        }

        preloadCacheReader.Close();
    }

    ~ShaderManagerImpl()
    {
#if HYP_ENABLE_SHADER_RELOAD
        StopShaderReloadThread();
#endif
    }

    static ShaderInstanceRef CompileShaderWork(
        Name shaderName,
        ShaderPropertySet properties,
        VertexInputLayoutDesc inputLayout,
        ShaderMapEntry* entry)
    {
        bool isValid = true;
        isValid &= g_shaderCompiler->RequestShader(shaderName, properties, inputLayout, entry->shader);

        isValid &= entry->shader->IsValid();

        if (!isValid)
        {
            // use fallback shader on fail.

            if (shaderName != s_nameFallbackShader
                && g_shaderCompiler->IsGraphicsShaderBundle(shaderName))
            {
                HYP_LOG(Shader, Verbose,
                        "Failed to compile shader '{}', trying to load fallback...",
                        shaderName);

                // @TODO Show editor alert.

                static constexpr StringHash PropertiesToKeepForFallback[] = {
                    "SKINNING"_sh,
                    "INSTANCING"_sh
                };

                ShaderPropertySet fallbackProperties {};

                for (const ShaderPropertyId shaderPropertyId : properties.ToArray())
                {
                    ShaderProperty shaderProperty;
                    if (GetShaderPropertyById(shaderPropertyId, shaderProperty)
                        && std::find(std::begin(PropertiesToKeepForFallback), std::end(PropertiesToKeepForFallback), shaderProperty.name) != std::end(PropertiesToKeepForFallback))
                    {
                        fallbackProperties.Add(shaderPropertyId);
                    }
                }

                isValid = g_shaderCompiler->RequestShader(s_nameFallbackShader, fallbackProperties, inputLayout, entry->shader);

                isValid &= entry->shader->IsValid();

                Assert(isValid, "Shader compilation failed and fallback shader could not be loaded!");
            }
            else
            {
                HYP_LOG(Shader, Error, "Failed to compile shader '{}'", shaderName);

                Assert(false, "Compiled shader '{}' is not a valid compiled shader", shaderName);
            }
        }

        if (!isValid)
        {
            return ShaderInstanceRef::Null();
        }

        ShaderInstanceRef si = RI.MakeShader(entry->shader);

        Check(si->Create());

        entry->shaderInstance = si;

        return si;
    }

    void IssueShaderCompile(
        ShaderMapEntry* entry,
        Name shaderName,
        const ShaderPropertySet& properties,
        const VertexInputLayoutDesc& inputLayout)
    {
        Assert(entry != nullptr);

        m_numCompilingShaders.Increment(1, MemoryOrder::RELAXED);
        m_totalNumCompilingShadersForTask.Increment(1, MemoryOrder::RELAXED);

#ifdef HYP_EDITOR
        {
            Mutex::Guard guard(m_compilingShadersMutex);

            ++m_compilingShaderNames[shaderName];

            UpdateEditorTask_Locked();
        }
#endif

        entry->compileTask = TaskSystem::GetInstance().Enqueue(
            [shaderName, properties, inputLayout, entry]() -> ShaderInstanceRef
            {
                return CompileShaderWork(shaderName, properties, inputLayout, entry);
            },
            TaskThreadPoolName::THREAD_POOL_BACKGROUND);

        entry->compileTask.OnComplete([this, shaderName](ShaderInstanceRef&)
            {
                m_numCompilingShaders.Decrement(1, MemoryOrder::RELAXED);

#ifdef HYP_EDITOR
                Mutex::Guard guard(m_compilingShadersMutex);

                auto it = m_compilingShaderNames.Find(shaderName);

                if (it != m_compilingShaderNames.End())
                {
                    if (--it->second == 0)
                    {
                        m_compilingShaderNames.Erase(it);
                    }
                }

                UpdateEditorTask_Locked();
#endif
            });
    }

#ifdef HYP_EDITOR
    // Must be called while holding m_compilingShadersMutex.
    void UpdateEditorTask_Locked()
    {
        auto getDescriptionText = [this]() -> String
        {
            String text;

            for (const auto& pair : m_compilingShaderNames)
            {
                text += HYP_FORMAT("{} ({})\n", pair.first, pair.second);
            }

            return text;
        };

        if (m_numCompilingShaders.Get(MemoryOrder::ACQUIRE) == 0)
        {
            m_editorTask.Access([](EditorTaskScope& task)
                {
                    task.Reset();
                });

            m_totalNumCompilingShadersForTask.Set(0, MemoryOrder::RELEASE);

            return;
        }

        const uint32 totalNumCompilingShaders = m_totalNumCompilingShadersForTask.Get(MemoryOrder::RELAXED);
        const uint32 numCompilingShaders = m_numCompilingShaders.Get(MemoryOrder::RELAXED);
        const uint32 numCompleted = totalNumCompilingShaders - numCompilingShaders;

        m_editorTask.Access([&](EditorTaskScope& task)
            {
                if (!task.GetEditorTask())
                {
                    task = EditorTaskScope(
                        TickableEditorTask::StaticClass(),
                        []()
                        { /* no tick function */ },
                        "Preparing shaders",
                        getDescriptionText(),
                        /* isForegroundTask */ true);
                }
                else
                {
                    task.GetEditorTask()->SetDescription(getDescriptionText());
                }

                task.GetEditorTask()->SetProgress(static_cast<float>(numCompleted) / static_cast<float>(totalNumCompilingShaders));
            });
    }
#endif

    void AddToPreloadCache(
        const Name name,
        const ShaderPropertySet& properties,
        const VertexInputLayoutDesc& inputLayout)
    {
#ifdef HYP_GENERATE_SHADER_PRELOAD_CACHE
        Mutex::Guard guard(m_cacheWriteMutex);

        ShaderPreloadEntry preloadEntry;
        Memory::Zero(&preloadEntry, sizeof(ShaderPreloadEntry));

        const char* nameStr = name.LookupString();
        Memory::CopyString(preloadEntry.nameStr, nameStr, MathUtil::Min(Memory::StrLen(nameStr) + 1, sizeof(preloadEntry.nameStr)));

        preloadEntry.properties = properties;
        preloadEntry.inputLayout = inputLayout;

        const HashCode hashCode = HashCode::GetHashCode(
            reinterpret_cast<const ubyte*>(&preloadEntry),
            reinterpret_cast<const ubyte*>(&preloadEntry) + sizeof(ShaderPreloadEntry));

        if (m_shaderPreloadEntriesToWriteHashes.Contains(hashCode))
        {
            // already contains; no need to add it/save it.
            return;
        }

        m_shaderPreloadEntriesToWriteHashes.Add(hashCode);
        m_shaderPreloadEntriesToWrite.PushBack(preloadEntry);

        const FilePath& cacheDir = EngineGlobals::GetCacheDirectory();

        FileByteWriter writer(cacheDir / "shaderpreload.bin", "wb+");
        writer.Write(m_shaderPreloadEntriesToWrite.Data(), m_shaderPreloadEntriesToWrite.Size() * sizeof(ShaderPreloadEntry));
        writer.Close();
#endif
    }

    ShaderMapEntry* FindOrCreateEntry(
        Name name,
        const ShaderPropertySet& properties,
        const VertexInputLayoutDesc& inputLayout,
        ShaderCacheId& outCacheId,
        bool doLoadShader)
    {
        const HashCode hc = GetShaderEntryHashCode(name, properties, inputLayout);

        outCacheId = InvalidShaderCacheId;

        { // fast path: entry already exists
            TSharedLock lock(m_mutex);

            auto it = m_entryMap.Find(hc);

            if (it != m_entryMap.End())
            {
                outCacheId = it->second->cacheId;

                return it->second;
            }
        }

        const ShaderCacheId cacheId = GenerateShaderCacheId();

        ShaderMapEntry* entry = GetShaderMapEntry(cacheId);

        Assert(!entry->IsLoaded());

        entry->cacheId = cacheId;
        entry->shader = GetShader(cacheId);

        Assert(!entry->shader->expired);

        if (doLoadShader)
        {
            IssueShaderCompile(entry, name, properties, inputLayout);
        }

        TUniqueLock lock(m_mutex);

        auto it = m_entryMap.Find(hc);

        if (it != m_entryMap.End())
        {
            outCacheId = it->second->cacheId;

            return it->second;
        }

        m_entryMap[hc] = entry;
        outCacheId = cacheId;

        return entry;
    }

    ShaderInstanceRef GetOrCreate(
        Name name, const ShaderPropertySet& properties, const VertexInputLayoutDesc& inputLayout,
        ShaderCacheId& outCacheId,
        const bool doLoadShader,
        const bool waitForCompile = true)
    {
        HYP_NAMED_SCOPE("Get shader from cache or create");

        const auto ensureMatch = [](const ShaderPropertySet& expectedProperties, const VertexInputLayoutDesc& expectedInputLayout, const Shader& received) -> bool
        {
            if (received.inputLayout.mask != expectedInputLayout.mask)
            {
                return false;
            }

            uint32 chunkOffset = 0;
            for (uint32 chunk : expectedProperties.chunks)
            {
                FOR_EACH_BIT(chunk, bit)
                {
                    ShaderPropertyId propertyId = ShaderPropertyId(chunkOffset + bit);

                    if (!received.properties.Test(propertyId))
                    {
                        return false;
                    }
                }

                chunkOffset += ShaderPropertySet::ChunkSizeBits;
            }

            return true;
        };

        ShaderMapEntry* entry = FindOrCreateEntry(name, properties, inputLayout, outCacheId, doLoadShader);

        if (!doLoadShader)
        {
            return ShaderInstanceRef::Null();
        }

        if (!entry->compileTask.IsValid())
        {
            HYP_LOG(Shader, Error, "Shader entry for '{}' has no in-flight or completed compile", name);

            return ShaderInstanceRef::Null();
        }

        if (waitForCompile)
        {
            if (!entry->compileTask.IsCompleted())
            {
                HYP_LOG(Shader, Warning, "Blocking wait on shader load for {} {}", name, properties.GetDebugString());

                AddToPreloadCache(name, properties, inputLayout);
            }

            ENGINE_STAT_SCOPE(&s_statShaderCompilation);

            entry->compileTask.Await();
        }
        else if (!entry->compileTask.IsCompleted())
        {
            return ShaderInstanceRef::Null();
        }

        Assert(entry->shaderInstance.IsValid()
            && !entry->shaderInstance->GetShader()->expired);

        if (!ensureMatch(properties, inputLayout, *entry->shaderInstance->GetShader()))
        {
            HYP_LOG(Shader, Warning, "Loaded shader from cache (Name: {}) does not contain the requested properties! "
                                     "Expected properties: {}, Expected Input Layout: {} "
                                     "Actual properties: {}, Actual Input Layout: {}",
                    name, properties.GetDebugString(), inputLayout.GetDebugString(),
                    entry->shaderInstance->GetShader()->properties.GetDebugString(), entry->shaderInstance->GetShader()->inputLayout.GetDebugString());
        }

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

    void PreloadShaders(
        Span<const ShaderPreloadEntry> shadersToPreload,
        bool blockingWait,
        const ProcRef<void(uint64 current, uint64 total)>& callback = nullptr)
    {
        if (!shadersToPreload)
        {
            return;
        }

        Array<ShaderMapEntry*, ShaderAllocator> issuedEntries;
        issuedEntries.Reserve(shadersToPreload.Size());

        for (size_t i = 0; i < shadersToPreload.Size(); i++)
        {
            const ShaderPreloadEntry& entry = shadersToPreload[i];

            // Sanity check in case of corrupt data; we want to ensure we have a NUL terminator in the name string
            bool nulFound = false;
            for (size_t j = 0; j < sizeof(entry.nameStr); j++)
            {
                if (entry.nameStr[j] == '\0')
                {
                    nulFound = true;
                    break;
                }
            }

            if (!nulFound)
            {
                HYP_LOG(Shader, Warning, "Shader preload entry had a corrupt name string, skipping!");
                continue;
            }

            if (callback.IsValid())
            {
                callback(i, shadersToPreload.Size());
            }

            ShaderCacheId unusedCacheId;
            ShaderMapEntry* mapEntry = FindOrCreateEntry(
                CreateNameFromDynamicString(entry.nameStr), entry.properties, entry.inputLayout,
                unusedCacheId,
                /* doLoadShader */ true);

            if (mapEntry->compileTask.IsValid())
            {
                issuedEntries.PushBack(mapEntry);
            }
        }

        if (blockingWait)
        {
            ENGINE_STAT_SCOPE(&s_statShaderCompilation);

            for (ShaderMapEntry* mapEntry : issuedEntries)
            {
                mapEntry->compileTask.Await();
            }
        }

        if (callback.IsValid())
        {
            callback(shadersToPreload.Size(), shadersToPreload.Size());
        }
    }

    void PreloadShadersFromCacheFile(
        bool blockingWait,
        const ProcRef<void(uint64 current, uint64 total)>& callback = nullptr)
    {
        if (m_preloadCacheEntries.Empty())
        {
            return; // nothing to preload
        }

        HYP_LOG(Shader, Info, "Preloading {} shaders", m_preloadCacheEntries.Size());

        PreloadShaders(
            m_preloadCacheEntries.ToSpan(),
            /* blockingWait */ blockingWait,
            callback);
    }

    void WriteShaderCache(const FilePath& outDir)
    {
        Mutex::Guard guard(m_cacheWriteMutex);

        { // Save the shader property cache
            FileByteWriter writer { outDir / "shaderprops.bin" };
            WriteShaderPropertyDictionary(writer);
            writer.Close();
        }

#ifdef HYP_GENERATE_SHADER_PRELOAD_CACHE
        { // Save preload cache
            if (m_shaderPreloadEntriesToWrite.Empty())
            {
                return;
            }

            FileByteWriter writer(outDir / "shaderpreload.bin", "wb+");
            writer.Write(m_shaderPreloadEntriesToWrite.Data(), m_shaderPreloadEntriesToWrite.Size() * sizeof(ShaderPreloadEntry));
            writer.Close();
        }
#endif
    }

    void ExpireShaderEntries(const Shader* shader)
    {
        if (!shader)
        {
            return;
        }

        TUniqueLock lock(m_mutex);

        Array<HashCode, ShaderAllocator> entriesToRemove;

        for (const auto& it : m_entryMap)
        {
            ShaderMapEntry* entry = it.second;

            if (!entry)
            {
                continue;
            }

            const Shader* entryShader = entry->shaderInstance.IsValid()
                ? entry->shaderInstance->GetShader()
                : entry->shader;

            if (entryShader == shader && entry->IsLoaded())
            {
                entriesToRemove.PushBack(it.first);
            }
        }

        for (HashCode hc : entriesToRemove)
        {
            auto it = m_entryMap.Find(hc);

            if (it != m_entryMap.End())
            {
                it->second->shaderInstance = ShaderInstanceRef::Null();
                it->second->shader = nullptr;

                m_entryMap.Erase(hc);
            }
        }
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

        struct ReloadItem
        {
            ShaderMapEntry* entry;
            Name shaderName;
            ShaderPropertySet properties;
            VertexInputLayoutDesc inputLayout;
        };

        Array<ReloadItem, ShaderAllocator> items;

        {
            TSharedLock lock(m_mutex);

            for (const auto& it : m_entryMap)
            {
                ShaderMapEntry* entry = it.second;

                if (!entry || !entry->shader)
                {
                    continue;
                }

                if (!entry->shader->baseName)
                {
                    // entry was allocated but never compiled; nothing to reload
                    continue;
                }

                if (entry->shader->IsSaved())
                {
                    if (!g_shaderCompiler->IsShaderBundleOutdated(entry->shader->baseName))
                    {
                        continue;
                    }
                }

                items.PushBack(ReloadItem { entry, entry->shader->baseName, entry->shader->properties, entry->shader->inputLayout });
            }
        }

        if (items.Empty())
        {
            AtomicExchange(&m_isReloadingShaders, 0);

            return;
        }

        String shadersText;

        for (const ReloadItem& item : items)
        {
            shadersText += "\t";
            shadersText += item.shaderName.LookupString();
            shadersText += " - ";
            shadersText += item.properties.GetDebugString();

            if (&item != &items.Back())
            {
                shadersText += "\n";
            }
        }

        HYP_LOG(Shader, Info, "Reloading {} shaders\n{}", items.Size(), shadersText);

        for (ReloadItem& item : items)
        {
            IssueShaderCompile(item.entry, item.shaderName, item.properties, item.inputLayout);
        }

        Set<Shader*, ShaderAllocator> shadersToExpire;

        for (ReloadItem& item : items)
        {
            ENGINE_STAT_SCOPE(&s_statShaderCompilation);

            item.entry->compileTask.Await();

            Shader* shader = item.entry->shader;
            Assert(shader != nullptr);

            if (shader->IsSaved())
            {
                AssertDebug(!g_shaderCompiler->IsShaderBundleOutdated(shader->baseName));
            }

            shader->AddRef();
            shadersToExpire.Add(shader);

            AssertDebug(item.entry->IsLoaded());
        }

        if (shadersToExpire.Any())
        {
            auto expireOnRenderThread = [toExpire = std::move(shadersToExpire)]()
            {
                for (Shader* shader : toExpire)
                {
                    RI.graphicsPipelineCache->ExpirePipelinesForShader(shader);
                    RI.computePipelineCache->ExpirePipelinesForShader(shader);
                    RI.rayTracingPipelineCache->ExpirePipelinesForShader(shader);

                    RI.shaderManager->ExpireShaderEntries(shader);

                    shader->Release();
                }
            };

            if (IsOnThread(g_renderThread))
            {
                expireOnRenderThread();
            }
            else
            {
                GetThreadById(g_renderThread)->GetScheduler()
                    .Enqueue(expireOnRenderThread, TaskEnqueueFlags::FIRE_AND_FORGET);
            }
        }

        AtomicExchange(&m_isReloadingShaders, 0);
    }
#endif // HYP_ENABLE_SHADER_RELOAD
};

ShaderManager::ShaderManager()
    : m_impl(MakePimplWithAllocator<ShaderManagerImpl, ShaderAllocator>())
{
}

ShaderInstanceRef ShaderManager::GetOrCreate(Name name, const ShaderPropertySet& propertySet, const VertexInputLayoutDesc& inputLayout, bool waitForCompile)
{
    AssertDebug(name.IsValid());

    ShaderCacheId cacheId;
    return m_impl->GetOrCreate(name, propertySet, inputLayout, cacheId, /* doLoadShader */ true, waitForCompile);
}

void ShaderManager::PreloadShadersFromCacheFile(
    bool blockingWait,
    const ProcRef<void(uint64 current, uint64 total)>& callback)
{
    m_impl->PreloadShadersFromCacheFile(blockingWait, callback);
}

void ShaderManager::PreloadShaders(
    Span<const ShaderPreloadEntry> shadersToPreload,
    bool blockingWait,
    const ProcRef<void(uint64 current, uint64 total)>& callback)
{
    m_impl->PreloadShaders(shadersToPreload, blockingWait, callback);
}

void ShaderManager::WriteShaderCache(const FilePath& outDir)
{
    m_impl->WriteShaderCache(outDir);
}

void ShaderManager::ExpireShaderEntries(const Shader* shader)
{
    m_impl->ExpireShaderEntries(shader);
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
