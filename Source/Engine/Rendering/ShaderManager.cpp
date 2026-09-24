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

static EngineStatTimer s_statShaderCompilation("Rendering/CPU/TotalShaderCompilationTime", /* resetPerFrame */ false);

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

struct ShaderPreloadCacheHeader
{
    static constexpr uint32 Magic = 0x50535948; // "HYSP"
    static constexpr uint16 CurrentVersion = 1;

    uint32 magic = Magic;
    uint16 version = CurrentVersion;
    uint16 entrySize = uint16(sizeof(ShaderPreloadEntry));
    uint32 entryCount = 0;

    // Entries store ShaderPropertyIds, so they're only valid against the property dictionary they were written with
    uint32 propertyIdCount = 0;
    uint64 propertyDictionaryHash = 0;
};

static bool HasValidShaderName(const ShaderPreloadEntry& entry)
{
    for (size_t index = 0; index < sizeof(entry.nameStr); index++)
    {
        if (entry.nameStr[index] == '\0')
        {
            return index != 0;
        }
    }

    return false;
}

class ShaderManagerImpl
{
public:
    // compileTask is assigned once, under m_mutex exclusively, and never reassigned - reloads compile into a new entry and swap it in.
    // shader, shaderInstance and failedToLoad are written by the compile task, so only read them once compileTask has completed.
    struct ShaderMapEntry
    {
        ShaderCacheId cacheId = InvalidShaderCacheId;
        ShaderInstanceRef shaderInstance;
        Shader* shader = nullptr;

        // what was requested - may differ from the resolved shader, e.g. when the fallback is used
        Name name;
        ShaderPropertySet properties;
        VertexInputLayoutDesc inputLayout;

        Task<ShaderInstanceRef> compileTask;

        // set by the compile task when the requested shader couldn't be loaded (the fallback may be in use)
        bool failedToLoad = false;

        // set under m_mutex exclusively once the entry is no longer in m_entryMap; its shaderInstance has been released
        bool retired = false;

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

#ifdef HYP_ENABLE_SHADER_RELOAD
    static constexpr uint32 ShaderReloadIntervalMs = 3000;
    AtomicVar<bool> m_shaderReloadShouldStop { false };
    Task<void> m_shaderReloadTask;

    volatile int32 m_isReloadingShaders = 0;
#endif

    Mutex m_cacheWriteMutex;

#ifdef HYP_GENERATE_SHADER_PRELOAD_CACHE
    // keyed by GetShaderEntryHashCode()
    Map<HashCode, ShaderPreloadEntry, ShaderAllocator, HashTablePolicy::NotPooled> m_shaderPreloadEntriesToWrite;
#endif

    Array<ShaderPreloadEntry, ShaderAllocator> m_preloadCacheEntries;

    ShaderManagerImpl()
    {
        const FilePath preloadCachePath = EngineGlobals::GetCacheDirectory() / "shaderpreload.bin";

        FileByteReader preloadCacheReader(preloadCachePath);

        if (preloadCacheReader.Eof())
        {
            // Not found; just continue anyway - next run will save em
            HYP_LOG(Shader, Info, "No shaderpreload.bin found in cache directory ({})", preloadCachePath);

            return;
        }

        const bool isValid = ReadPreloadCache(preloadCacheReader);

        // must be closed before removing, the file can't be deleted while it's open on Windows
        preloadCacheReader.Close();

        if (!isValid)
        {
            m_preloadCacheEntries.Clear();

            if (!preloadCachePath.Remove())
            {
                HYP_LOG(Shader, Warning, "Shader preload cache file at {} could not be removed; ignoring", preloadCachePath);
            }
        }
    }

    bool ReadPreloadCache(ByteReader& reader)
    {
        ShaderPreloadCacheHeader header;

        if (reader.Read(static_cast<void*>(&header), sizeof(ShaderPreloadCacheHeader)) != sizeof(ShaderPreloadCacheHeader)
            || header.magic != ShaderPreloadCacheHeader::Magic
            || header.version != ShaderPreloadCacheHeader::CurrentVersion
            || header.entrySize != sizeof(ShaderPreloadEntry))
        {
            HYP_LOG(Shader, Warning, "Shader preload cache has an unrecognized header (old format?) - discarding it");

            return false;
        }

        if (reader.Max() - reader.Position() != size_t(header.entryCount) * sizeof(ShaderPreloadEntry))
        {
            HYP_LOG(Shader, Warning, "Shader preload cache size doesn't match its entry count ({}) - discarding it", header.entryCount);

            return false;
        }

        HashCode propertyDictionaryHashCode;

        if (!GetShaderPropertyDictionaryHashCode(header.propertyIdCount, propertyDictionaryHashCode)
            || propertyDictionaryHashCode.Value() != header.propertyDictionaryHash)
        {
            HYP_LOG(Shader, Warning, "Shader preload cache was written against a different shaderprops.bin - discarding it");

            return false;
        }

        Array<ShaderPreloadEntry, ShaderAllocator> fileEntries;
        fileEntries.Resize(header.entryCount);

        reader.Read(static_cast<void*>(fileEntries.Data()), fileEntries.Size() * sizeof(ShaderPreloadEntry));

        Set<HashCode, ShaderAllocator> seenHashCodes;
        m_preloadCacheEntries.Reserve(fileEntries.Size());

        for (const ShaderPreloadEntry& entry : fileEntries)
        {
            if (!HasValidShaderName(entry))
            {
                HYP_LOG(Shader, Warning, "Shader preload entry had a corrupt name string, skipping!");

                continue;
            }

            const HashCode hashCode = GetShaderEntryHashCode(CreateNameFromDynamicString(entry.nameStr), entry.properties, entry.inputLayout);

            if (!seenHashCodes.Insert(hashCode).second)
            {
                continue;
            }

            m_preloadCacheEntries.PushBack(entry);

#ifdef HYP_GENERATE_SHADER_PRELOAD_CACHE
            // carry loaded entries over so they aren't dropped the next time we save
            m_shaderPreloadEntriesToWrite.Insert(hashCode, entry);
#endif
        }

        return true;
    }

    ~ShaderManagerImpl()
    {
#ifdef HYP_ENABLE_SHADER_RELOAD
        StopShaderReloadThread();
#endif
    }

    ShaderInstanceRef CompileShaderWork(
        Name shaderName,
        ShaderPropertySet properties,
        VertexInputLayoutDesc inputLayout,
        ShaderMapEntry* entry)
    {
        bool isValid = true;
        isValid &= g_shaderCompiler->RequestShader(shaderName, properties, inputLayout, entry->shader);

        isValid &= entry->shader->IsValid();

        entry->failedToLoad = !isValid;

        if (!isValid)
        {
            // don't keep trying to preload a shader that can't be loaded
            RemoveFromPreloadCache(shaderName, properties, inputLayout);

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
                // not fatal here since preloads and reloads can recover - GetOrCreate() asserts when a real request gets no shader
                HYP_LOG(Shader, Error, "Failed to compile shader '{}' {}", shaderName, properties.GetDebugString());
            }
        }

        if (!isValid)
        {
            return ShaderInstanceRef::Null();
        }

        ShaderInstanceRef si = RI.MakeShader(entry->shader);

        if (RendererResult createResult = si->Create(); createResult.HasError())
        {
            // not fatal here since preloads and reloads can recover - GetOrCreate() asserts when a real request gets no shader
            HYP_LOG(Shader, Error, "Failed to create shader instance for '{}' {}: {}",
                    shaderName, properties.GetDebugString(), createResult.GetError().GetMessage());

            RemoveFromPreloadCache(shaderName, properties, inputLayout);

            entry->failedToLoad = true;

            return ShaderInstanceRef::Null();
        }

        entry->shaderInstance = si;

        return si;
    }

    // Must be called while holding m_mutex exclusively.
    ShaderMapEntry* CreateEntry_Locked(
        Name name,
        const ShaderPropertySet& properties,
        const VertexInputLayoutDesc& inputLayout)
    {
        const ShaderCacheId cacheId = GenerateShaderCacheId();

        ShaderMapEntry* entry = &*m_entries.Emplace(uint64(cacheId));
        entry->cacheId = cacheId;
        entry->shader = &*m_compiledShaderCache.Emplace(uint64(cacheId));
        entry->name = name;
        entry->properties = properties;
        entry->inputLayout = inputLayout;

        return entry;
    }

    // Must be called while holding m_mutex exclusively, on an entry whose compile has completed.
    // GetOrCreate() reads shaderInstance under the shared lock and looks the shader up again when it sees a retired entry.
    // Returns the released instance so the caller can choose which thread drops it.
    ShaderInstanceRef RetireEntry_Locked(ShaderMapEntry* entry)
    {
        ShaderInstanceRef releasedInstance = std::move(entry->shaderInstance);

        entry->shaderInstance = ShaderInstanceRef::Null();
        entry->retired = true;

        return releasedInstance;
    }

    // Must be called while holding m_mutex exclusively, since readers check entry->compileTask under the shared lock.
    void EnqueueShaderCompile(
        ShaderMapEntry* entry,
        Name shaderName,
        const ShaderPropertySet& properties,
        const VertexInputLayoutDesc& inputLayout)
    {
        Assert(entry != nullptr);

        entry->compileTask = TaskSystem::GetInstance().Enqueue(
            [this, shaderName, properties, inputLayout, entry]() -> ShaderInstanceRef
            {
                return CompileShaderWork(shaderName, properties, inputLayout, entry);
            },
            TaskThreadPoolName::THREAD_POOL_BACKGROUND);
    }

    void TrackShaderCompile(ShaderMapEntry* entry, Name shaderName)
    {
        Assert(entry != nullptr && entry->compileTask.IsValid());

#ifdef HYP_EDITOR
        {
            Mutex::Guard guard(m_compilingShadersMutex);

            m_numCompilingShaders.Increment(1, MemoryOrder::RELAXED);
            m_totalNumCompilingShadersForTask.Increment(1, MemoryOrder::RELAXED);

            ++m_compilingShaderNames[shaderName];

            UpdateEditorTask_Locked();
        }
#else
        m_numCompilingShaders.Increment(1, MemoryOrder::RELAXED);
        m_totalNumCompilingShadersForTask.Increment(1, MemoryOrder::RELAXED);
#endif

        entry->compileTask.OnComplete([this, shaderName](ShaderInstanceRef&)
            {
#ifdef HYP_EDITOR
                Mutex::Guard guard(m_compilingShadersMutex);

                m_numCompilingShaders.Decrement(1, MemoryOrder::RELAXED);

                auto it = m_compilingShaderNames.Find(shaderName);

                if (it != m_compilingShaderNames.End())
                {
                    if (--it->second == 0)
                    {
                        m_compilingShaderNames.Erase(it);
                    }
                }

                UpdateEditorTask_Locked();
#else
                m_numCompilingShaders.Decrement(1, MemoryOrder::RELAXED);
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
        const char* nameStr = name.LookupString();
        const size_t nameLength = Memory::StrLen(nameStr);

        ShaderPreloadEntry preloadEntry;

        if (nameLength >= sizeof(preloadEntry.nameStr))
        {
            HYP_LOG(Shader, Warning, "Shader name '{}' is too long to be stored in the preload cache", name);

            return;
        }

        const HashCode hashCode = GetShaderEntryHashCode(name, properties, inputLayout);

        Mutex::Guard guard(m_cacheWriteMutex);

        if (m_shaderPreloadEntriesToWrite.Contains(hashCode))
        {
            return;
        }

        Memory::Zero(&preloadEntry, sizeof(ShaderPreloadEntry));
        Memory::Copy(preloadEntry.nameStr, nameStr, nameLength);

        preloadEntry.properties = properties;
        preloadEntry.inputLayout = inputLayout;

        m_shaderPreloadEntriesToWrite.Insert(hashCode, preloadEntry);
#endif
    }

    void RemoveFromPreloadCache(
        const Name name,
        const ShaderPropertySet& properties,
        const VertexInputLayoutDesc& inputLayout)
    {
#ifdef HYP_GENERATE_SHADER_PRELOAD_CACHE
        const HashCode hashCode = GetShaderEntryHashCode(name, properties, inputLayout);

        Mutex::Guard guard(m_cacheWriteMutex);

        if (m_shaderPreloadEntriesToWrite.Erase(hashCode))
        {
            HYP_LOG(Shader, Info, "Removed shader '{}' {} from the preload cache since it failed to load", name, properties.GetDebugString());
        }
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

        { // fast path: entry already exists
            TSharedLock lock(m_mutex);

            auto it = m_entryMap.Find(hc);

            if (it != m_entryMap.End() && (!doLoadShader || it->second->compileTask.IsValid()))
            {
                outCacheId = it->second->cacheId;

                return it->second;
            }
        }

        ShaderMapEntry* entry = nullptr;
        bool issuedCompile = false;

        {
            // Creating the entry and enqueuing its compile under one exclusive lock means concurrent
            // requests for the same shader (e.g. preload vs. render thread) share a single compile.
            TUniqueLock lock(m_mutex);

            auto it = m_entryMap.Find(hc);

            if (it != m_entryMap.End())
            {
                entry = it->second;
            }
            else
            {
                entry = CreateEntry_Locked(name, properties, inputLayout);

                m_entryMap[hc] = entry;
            }

            if (doLoadShader && !entry->compileTask.IsValid())
            {
                EnqueueShaderCompile(entry, name, properties, inputLayout);

                issuedCompile = true;
            }

            outCacheId = entry->cacheId;
        }

        if (issuedCompile)
        {
            TrackShaderCompile(entry, name);
        }

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

        while (true)
        {
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
                const bool isBlockingWait = !entry->compileTask.IsCompleted();

                if (isBlockingWait)
                {
                    HYP_LOG(Shader, Warning, "Blocking wait on shader load for {} {}", name, properties.GetDebugString());
                }

                ENGINE_STAT_SCOPE(&s_statShaderCompilation);

                entry->compileTask.Await();

                // only recorded once it's known to load, so shaders that fail don't get preloaded next run
                if (isBlockingWait && !entry->failedToLoad)
                {
                    AddToPreloadCache(name, properties, inputLayout);
                }
            }
            else if (!entry->compileTask.IsCompleted())
            {
                return ShaderInstanceRef::Null();
            }

            // held while touching the shader instance so a reload or ExpireShaderEntries() can't retire it (and free the shader) underneath us
            TSharedLock lock(m_mutex);

            if (entry->retired)
            {
                // replaced by a reload or expired after we found it - look it up again
                continue;
            }

            Assert(entry->shaderInstance.IsValid(), "Compiled shader '{}' is not a valid compiled shader", name);

            if (!entry->shaderInstance.IsValid())
            {
                return ShaderInstanceRef::Null();
            }

            Assert(!entry->shaderInstance->GetShader()->expired);

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

    void PreloadShaders(
        Span<const ShaderPreloadEntry> shadersToPreload,
        bool blockingWait,
        const ProcRef<void(uint64 current, uint64 total)>& callback = nullptr)
    {
        if (!shadersToPreload)
        {
            return;
        }

        const uint64 numShaders = shadersToPreload.Size();

        Array<ShaderMapEntry*, ShaderAllocator> issuedEntries;
        issuedEntries.Reserve(numShaders);

        ShaderPropertySet globalProperties;
        MergeGlobalShaderProperties(globalProperties);

        Array<Name, ShaderAllocator> globalPropertyNames;

        for (const ShaderPropertyId propertyId : globalProperties.ToArray())
        {
            ShaderProperty property;

            if (GetShaderPropertyById(propertyId, property))
            {
                globalPropertyNames.PushBack(property.name);
            }
        }

        // Entries are recorded with the global properties merged in, and Cache/ is shared between builds. Entries from a build with
        // another backend or platform (e.g. BACKEND=VULKAN on a DX12 build) are left in the cache for that build, but not preloaded here.
        const auto isRecordedForOtherTarget = [&](const ShaderPreloadEntry& preloadEntry) -> bool
        {
            for (const ShaderPropertyId propertyId : preloadEntry.properties.ToArray())
            {
                ShaderProperty property;

                if (!globalProperties.Test(propertyId)
                    && GetShaderPropertyById(propertyId, property)
                    && globalPropertyNames.Contains(property.name))
                {
                    return true;
                }
            }

            return false;
        };

        uint64 numOtherTargetEntries = 0;

        for (const ShaderPreloadEntry& preloadEntry : shadersToPreload)
        {
            if (!HasValidShaderName(preloadEntry))
            {
                HYP_LOG(Shader, Warning, "Shader preload entry had a corrupt name string, skipping!");

                continue;
            }

            if (isRecordedForOtherTarget(preloadEntry))
            {
                ++numOtherTargetEntries;

                continue;
            }

            const Name shaderName = CreateNameFromDynamicString(preloadEntry.nameStr);

            if (!g_shaderCompiler->HasShaderBundle(shaderName))
            {
                HYP_LOG(Shader, Warning, "Shader preload entry references unknown shader '{}', skipping", shaderName);

                RemoveFromPreloadCache(shaderName, preloadEntry.properties, preloadEntry.inputLayout);

                continue;
            }

            ShaderCacheId unusedCacheId;
            ShaderMapEntry* mapEntry = FindOrCreateEntry(
                shaderName, preloadEntry.properties, preloadEntry.inputLayout,
                unusedCacheId,
                /* doLoadShader */ true);

            issuedEntries.PushBack(mapEntry);
        }

        if (numOtherTargetEntries != 0)
        {
            HYP_LOG(Shader, Info, "Skipped {} shader preload entries recorded with a different backend or platform", numOtherTargetEntries);
        }

        if (blockingWait)
        {
            ENGINE_STAT_SCOPE(&s_statShaderCompilation);

            const uint64 numSkipped = numShaders - issuedEntries.Size();

            for (size_t index = 0; index < issuedEntries.Size(); index++)
            {
                issuedEntries[index]->compileTask.Await();

                if (callback.IsValid())
                {
                    callback(numSkipped + index + 1, numShaders);
                }
            }
        }

        if (callback.IsValid())
        {
            callback(numShaders, numShaders);
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

#ifdef HYP_GENERATE_SHADER_PRELOAD_CACHE
        // Taken before writing the dictionary so every property id the preload entries use is in the written file
        ShaderPreloadCacheHeader preloadCacheHeader;
        preloadCacheHeader.propertyIdCount = GetShaderPropertyCount();

        HashCode propertyDictionaryHashCode;
        GetShaderPropertyDictionaryHashCode(preloadCacheHeader.propertyIdCount, propertyDictionaryHashCode);

        preloadCacheHeader.propertyDictionaryHash = propertyDictionaryHashCode.Value();
#endif

        { // Save the shader property cache
            FileByteWriter writer { outDir / "shaderprops.bin" };
            WriteShaderPropertyDictionary(writer);
            writer.Close();
        }

#ifdef HYP_GENERATE_SHADER_PRELOAD_CACHE
        { // Save preload cache - always written (even when empty) so it can't be left paired with a newer shaderprops.bin
            Array<ShaderPreloadEntry, ShaderAllocator> preloadEntries;
            preloadEntries.Reserve(m_shaderPreloadEntriesToWrite.Size());

            for (const auto& it : m_shaderPreloadEntriesToWrite)
            {
                preloadEntries.PushBack(it.second);
            }

            preloadCacheHeader.entryCount = uint32(preloadEntries.Size());

            FileByteWriter writer(outDir / "shaderpreload.bin");
            writer.Write(&preloadCacheHeader, sizeof(ShaderPreloadCacheHeader));
            writer.Write(preloadEntries.Data(), preloadEntries.Size() * sizeof(ShaderPreloadEntry));
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

            // entries still compiling are skipped - the compile task is still writing shader and shaderInstance
            if (!entry || !entry->IsLoaded())
            {
                continue;
            }

            const Shader* entryShader = entry->shaderInstance.IsValid()
                ? entry->shaderInstance->GetShader()
                : entry->shader;

            if (entryShader == shader)
            {
                entriesToRemove.PushBack(it.first);
            }
        }

        for (HashCode hc : entriesToRemove)
        {
            auto it = m_entryMap.Find(hc);

            if (it != m_entryMap.End())
            {
                RetireEntry_Locked(it->second);

                m_entryMap.Erase(it);
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

            const ShaderMapEntry* entry = it.second;

            if (entry && entry->IsLoaded() && entry->shaderInstance.IsValid())
            {
                for (const BlobDataReference& blob : entry->shaderInstance->GetShader()->shaderBlobs)
                {
                    totalMemoryUsage += blob.size;
                }
            }
        }

        return totalMemoryUsage;
    }

#ifdef HYP_ENABLE_SHADER_RELOAD
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

        // Reloads compile into a new entry that isn't visible until it's swapped into m_entryMap, so the old entry is never
        // mutated while other threads may be awaiting or reading it, and keeps serving its shader until the new one is ready.
        struct ReloadItem
        {
            HashCode hashCode;
            ShaderMapEntry* oldEntry = nullptr;
            ShaderMapEntry* newEntry = nullptr;
            Handle<Shader> oldShader;
        };

        Array<ReloadItem, ShaderAllocator> items;

        {
            TSharedLock lock(m_mutex);

            for (const auto& it : m_entryMap)
            {
                ShaderMapEntry* entry = it.second;

                // entries still compiling are skipped - the compile task is still writing entry->shader
                if (!entry || !entry->IsLoaded() || !entry->shader)
                {
                    continue;
                }

                if (!entry->shader->baseName)
                {
                    // compile didn't resolve a shader; nothing to reload
                    continue;
                }

                if (entry->shader->IsSaved())
                {
                    if (!g_shaderCompiler->IsShaderOutdated(*entry->shader))
                    {
                        continue;
                    }
                }

                items.PushBack(ReloadItem { it.first, entry, nullptr, MakeStrongRef(entry->shader) });
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
            shadersText += item.oldEntry->name.LookupString();
            shadersText += " - ";
            shadersText += item.oldEntry->properties.GetDebugString();

            if (&item != &items.Back())
            {
                shadersText += "\n";
            }
        }

        HYP_LOG(Shader, Info, "Reloading {} shaders\n{}", items.Size(), shadersText);

        {
            TUniqueLock lock(m_mutex);

            for (ReloadItem& item : items)
            {
                item.newEntry = CreateEntry_Locked(item.oldEntry->name, item.oldEntry->properties, item.oldEntry->inputLayout);

                EnqueueShaderCompile(item.newEntry, item.newEntry->name, item.newEntry->properties, item.newEntry->inputLayout);
            }
        }

        for (ReloadItem& item : items)
        {
            TrackShaderCompile(item.newEntry, item.newEntry->name);
        }

        for (ReloadItem& item : items)
        {
            ENGINE_STAT_SCOPE(&s_statShaderCompilation);

            item.newEntry->compileTask.Await();

            if (!item.newEntry->failedToLoad && item.newEntry->shader->IsSaved())
            {
                AssertDebug(!g_shaderCompiler->IsShaderOutdated(*item.newEntry->shader));
            }
        }

        Set<Handle<Shader>, ShaderAllocator> shadersToExpire;

        // shader instances are released on the render thread, same as ExpireShaderEntries()
        Array<ShaderInstanceRef, ShaderAllocator> instancesToRelease;

        {
            TUniqueLock lock(m_mutex);

            for (ReloadItem& item : items)
            {
                if (!item.newEntry->shaderInstance.IsValid())
                {
                    // reload failed outright - keep serving the old shader
                    item.newEntry->retired = true;

                    continue;
                }

                auto it = m_entryMap.Find(item.hashCode);

                if (it == m_entryMap.End())
                {
                    // already expired, e.g. by the shader compiler replacing the old shader while we compiled
                    m_entryMap[item.hashCode] = item.newEntry;
                }
                else if (it->second == item.oldEntry)
                {
                    instancesToRelease.PushBack(RetireEntry_Locked(item.oldEntry));

                    it->second = item.newEntry;
                }
                else
                {
                    // expired and requested again while we compiled - that entry was compiled from the new source too
                    instancesToRelease.PushBack(RetireEntry_Locked(item.newEntry));
                }

                if (item.oldShader.IsValid() && item.oldShader.Get() != item.newEntry->shader)
                {
                    shadersToExpire.Add(std::move(item.oldShader));
                }
            }
        }

        if (shadersToExpire.Any() || instancesToRelease.Any())
        {
            auto expireOnRenderThread = [toExpire = std::move(shadersToExpire), toRelease = std::move(instancesToRelease)]() mutable
            {
                for (const Handle<Shader>& shader : toExpire)
                {
                    if (shader->expired)
                    {
                        continue;
                    }

                    RI.graphicsPipelineCache->ExpirePipelinesForShader(shader);
                    RI.computePipelineCache->ExpirePipelinesForShader(shader);
                    RI.rayTracingPipelineCache->ExpirePipelinesForShader(shader);

                    g_shaderManager->ExpireShaderEntries(shader);
                }

                toRelease.Clear();
            };

            if (IsOnThread(g_renderThread))
            {
                expireOnRenderThread();
            }
            else
            {
                // moved so no copy of the released instances is left to be dropped on this thread
                GetThreadById(g_renderThread)->GetScheduler()
                    .Enqueue(std::move(expireOnRenderThread), TaskEnqueueFlags::FIRE_AND_FORGET);
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

void ShaderManager::StartShaderReloadThread()
{
#ifdef HYP_ENABLE_SHADER_RELOAD
    m_impl->StartShaderReloadThread();
#endif // HYP_ENABLE_SHADER_RELOAD
}

void ShaderManager::StopShaderReloadThread()
{
#ifdef HYP_ENABLE_SHADER_RELOAD
    m_impl->StopShaderReloadThread();
#endif // HYP_ENABLE_SHADER_RELOAD
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
