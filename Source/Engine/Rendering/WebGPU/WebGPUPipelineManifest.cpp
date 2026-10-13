/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#include <WebGPUPch.hpp>

#include <Rendering/WebGPU/WebGPUPipelineManifest.hpp>
#include <Rendering/WebGPU/WebGPUComputePipeline.hpp>
#include <Rendering/WebGPU/WebGPUShaderInstance.hpp>
#include <Rendering/WebGPU/WebGPURenderInterface.hpp>
#include <Rendering/WebGPU/WebGPUHelpers.hpp>

#include <Rendering/GenericPipelineCache.hpp>

#include <Rendering/Util/ShaderPropertyDictionary.hpp>

#include <Core/IO/ByteWriter.hpp>

#include <Core/Logging/Logger.hpp>

namespace Hyperion {

ENGINE_API HYP_DECLARE_LOG_CHANNEL(RenderingBackend);

extern WebGPURenderInterface RI;

namespace {

enum WebGPUPipelineManifestBindingKind : uint8
{
    WPMBK_BUFFER,
    WPMBK_SAMPLER,
    WPMBK_TEXTURE,
    WPMBK_STORAGE_TEXTURE
};

static constexpr WGPUBufferBindingType BufferBindingTypes[] = {
    WGPUBufferBindingType_Uniform,
    WGPUBufferBindingType_Storage,
    WGPUBufferBindingType_ReadOnlyStorage
};

static constexpr WGPUSamplerBindingType SamplerBindingTypes[] = {
    WGPUSamplerBindingType_Filtering,
    WGPUSamplerBindingType_NonFiltering,
    WGPUSamplerBindingType_Comparison
};

static constexpr WGPUTextureSampleType TextureSampleTypes[] = {
    WGPUTextureSampleType_Float,
    WGPUTextureSampleType_UnfilterableFloat,
    WGPUTextureSampleType_Depth,
    WGPUTextureSampleType_Sint,
    WGPUTextureSampleType_Uint
};

static constexpr WGPUStorageTextureAccess StorageTextureAccesses[] = {
    WGPUStorageTextureAccess_WriteOnly,
    WGPUStorageTextureAccess_ReadOnly,
    WGPUStorageTextureAccess_ReadWrite
};

static constexpr WGPUTextureViewDimension TextureViewDimensions[] = {
    WGPUTextureViewDimension_1D,
    WGPUTextureViewDimension_2D,
    WGPUTextureViewDimension_2DArray,
    WGPUTextureViewDimension_Cube,
    WGPUTextureViewDimension_CubeArray,
    WGPUTextureViewDimension_3D
};

template <class T, size_t N>
constexpr bool ToFileValue(const T (&values)[N], T value, uint8& outFileValue)
{
    for (size_t index = 0; index < N; index++)
    {
        if (values[index] == value)
        {
            outFileValue = uint8(index);

            return true;
        }
    }

    return false;
}

template <class T, size_t N>
constexpr bool FromFileValue(const T (&values)[N], uint8 fileValue, T& outValue)
{
    if (fileValue >= N)
    {
        return false;
    }

    outValue = values[fileValue];

    return true;
}

constexpr bool ToFileFormat(WGPUTextureFormat format, uint8& outFileFormat)
{
    for (uint32 formatIndex = 0; formatIndex <= 0xFFu; formatIndex++)
    {
        if (ToWGPUTextureFormat(TextureFormat(formatIndex)) == format)
        {
            outFileFormat = uint8(formatIndex);

            return true;
        }
    }

    return false;
}

bool ToManifestBinding(const WGPUBindGroupLayoutEntry& layoutEntry, uint32 group, WebGPUPipelineManifestBinding& outBinding)
{
    static constexpr WGPUBindGroupLayoutEntry UnusedEntry = WGPU_BIND_GROUP_LAYOUT_ENTRY_INIT;

    if (group > 0xFFu || layoutEntry.binding > 0xFFFFu)
    {
        return false;
    }

    outBinding = {};
    outBinding.group = uint8(group);
    outBinding.binding = uint16(layoutEntry.binding);
    outBinding.visibility = uint8(layoutEntry.visibility);

    if (layoutEntry.buffer.type != UnusedEntry.buffer.type)
    {
        outBinding.kind = WPMBK_BUFFER;
        outBinding.viewDimension = layoutEntry.buffer.hasDynamicOffset ? 1 : 0;

        return ToFileValue(BufferBindingTypes, layoutEntry.buffer.type, outBinding.type);
    }

    if (layoutEntry.sampler.type != UnusedEntry.sampler.type)
    {
        outBinding.kind = WPMBK_SAMPLER;

        return ToFileValue(SamplerBindingTypes, layoutEntry.sampler.type, outBinding.type);
    }

    if (layoutEntry.storageTexture.access != UnusedEntry.storageTexture.access)
    {
        outBinding.kind = WPMBK_STORAGE_TEXTURE;

        return ToFileValue(StorageTextureAccesses, layoutEntry.storageTexture.access, outBinding.type)
            && ToFileValue(TextureViewDimensions, layoutEntry.storageTexture.viewDimension, outBinding.viewDimension)
            && ToFileFormat(layoutEntry.storageTexture.format, outBinding.format);
    }

    outBinding.kind = WPMBK_TEXTURE;

    return ToFileValue(TextureSampleTypes, layoutEntry.texture.sampleType, outBinding.type)
        && ToFileValue(TextureViewDimensions, layoutEntry.texture.viewDimension, outBinding.viewDimension);
}

bool ToLayoutEntry(const WebGPUPipelineManifestBinding& binding, WGPUBindGroupLayoutEntry& outLayoutEntry)
{
    outLayoutEntry = WGPU_BIND_GROUP_LAYOUT_ENTRY_INIT;
    outLayoutEntry.binding = binding.binding;
    outLayoutEntry.visibility = WGPUShaderStage(binding.visibility);

    switch (binding.kind)
    {
    case WPMBK_BUFFER:
        outLayoutEntry.buffer.hasDynamicOffset = binding.viewDimension != 0;

        return FromFileValue(BufferBindingTypes, binding.type, outLayoutEntry.buffer.type);
    case WPMBK_SAMPLER:
        return FromFileValue(SamplerBindingTypes, binding.type, outLayoutEntry.sampler.type);
    case WPMBK_TEXTURE:
        return FromFileValue(TextureSampleTypes, binding.type, outLayoutEntry.texture.sampleType)
            && FromFileValue(TextureViewDimensions, binding.viewDimension, outLayoutEntry.texture.viewDimension);
    case WPMBK_STORAGE_TEXTURE:
        outLayoutEntry.storageTexture.format = ToWGPUTextureFormat(TextureFormat(binding.format));

        return outLayoutEntry.storageTexture.format != WGPUTextureFormat_Undefined
            && FromFileValue(StorageTextureAccesses, binding.type, outLayoutEntry.storageTexture.access)
            && FromFileValue(TextureViewDimensions, binding.viewDimension, outLayoutEntry.storageTexture.viewDimension);
    default:
        return false;
    }
}

////////////////////


struct WebGPUPipelineManifestHeader
{
    static constexpr uint32 Magic = 0x56505948; // "HYPV"
    static constexpr uint16 CurrentVersion = 1;

    uint32 magic = Magic;
    uint16 version = CurrentVersion;
    uint16 float32Filterable = 0;
    uint32 entryCount = 0;

    // Entries store ShaderPropertyIds, so they're only valid against the property dictionary they were written with
    uint32 propertyIdCount = 0;
    uint64 propertyDictionaryHash = 0;
};

struct WebGPUPipelineManifestEntryHeader
{
    char nameStr[128];
    ShaderPropertySet properties;
    uint32 numBindings;
};

} // namespace 


#pragma region WebGPUPipelineManifest

WebGPUPipelineManifest::WebGPUPipelineManifest() = default;

WebGPUPipelineManifest::~WebGPUPipelineManifest() = default;

uint64 WebGPUPipelineManifest::GetEntryKey(const Entry& entry)
{
    HashCode hashCode = entry.shaderName.GetHashCode().Combine(entry.properties.GetHashCode());

    for (const WebGPUPipelineManifestBinding& binding : entry.bindings)
    {
        hashCode.Add(binding.group);
        hashCode.Add(binding.kind);
        hashCode.Add(binding.visibility);
        hashCode.Add(binding.type);
        hashCode.Add(binding.viewDimension);
        hashCode.Add(binding.format);
        hashCode.Add(binding.binding);
    }

    return hashCode.Value();
}

void WebGPUPipelineManifest::Record(const ShaderDesc& shaderDesc, const WGPUBindGroupLayout* layouts, uint32 numLayouts)
{
    if (m_isPrewarming || !shaderDesc.name.IsValid())
    {
        return;
    }

    Entry entry;
    entry.shaderName = shaderDesc.name;
    entry.properties = shaderDesc.properties;

    Array<WGPUBindGroupLayoutEntry, WebGPUAllocator> layoutEntries;

    for (uint32 layoutIndex = 0; layoutIndex < numLayouts; layoutIndex++)
    {
        layoutEntries.Clear();

        if (!RI.GetBindGroupLayoutEntries(layouts[layoutIndex], layoutEntries))
        {
            return;
        }

        for (const WGPUBindGroupLayoutEntry& layoutEntry : layoutEntries)
        {
            WebGPUPipelineManifestBinding binding;

            if (!ToManifestBinding(layoutEntry, layoutIndex, binding))
            {
                HYP_LOG_ONCE(RenderingBackend, Warning, "A variant of {} has a binding the pipeline manifest can't hold, it is left out", shaderDesc.name);

                return;
            }

            entry.bindings.PushBack(binding);
        }
    }

    entry.key = GetEntryKey(entry);

    // the pipeline exists already, it is the one being recorded
    entry.isPrewarmed = true;

    for (const Entry& existingEntry : m_entries)
    {
        if (existingEntry.key == entry.key)
        {
            // still held by the manifest, so nothing has used the variant made from it: this one was made instead
            if (existingEntry.pipeline.IsValid())
            {
                HYP_LOG(RenderingBackend, Warning, "Pipeline manifest variant of {} was compiled ahead of use but is not the one being used", shaderDesc.name);
            }

            return;
        }
    }

    m_entries.PushBack(std::move(entry));
}

void WebGPUPipelineManifest::Load()
{
    const FilePath filePath = EngineGlobals::GetCacheDirectory() / FileName;

    FileByteReader reader(filePath);

    if (reader.Eof())
    {
        return;
    }

    if (!Read(reader))
    {
        HYP_LOG(RenderingBackend, Warning, "Pipeline manifest at {} is not usable, pipelines will compile as they are first used", filePath);

        m_entries.Clear();
    }

    reader.Close();

    HYP_LOG(RenderingBackend, Info, "Pipeline manifest holds {} variant(s) to compile ahead of use", m_entries.Size());

    m_hasReported = m_entries.Empty();
}

bool WebGPUPipelineManifest::Read(ByteReader& reader)
{
    WebGPUPipelineManifestHeader header;

    if (reader.Read(static_cast<void*>(&header), sizeof(header)) != sizeof(header)
        || header.magic != WebGPUPipelineManifestHeader::Magic
        || header.version != WebGPUPipelineManifestHeader::CurrentVersion)
    {
        return false;
    }

    HashCode propertyDictionaryHashCode;

    if (!GetShaderPropertyDictionaryHashCode(header.propertyIdCount, propertyDictionaryHashCode)
        || propertyDictionaryHashCode.Value() != header.propertyDictionaryHash)
    {
        return false;
    }

    if (bool(header.float32Filterable) != RI.GetDeviceFeatures().float32Filterable)
    {
        HYP_LOG(RenderingBackend, Warning, "Pipeline manifest was recorded {} filterable 32 bit float textures and this device is the opposite, so some of its variants will not be the ones used",
            header.float32Filterable ? "with" : "without");
    }

    for (uint32 entryIndex = 0; entryIndex < header.entryCount; entryIndex++)
    {
        WebGPUPipelineManifestEntryHeader entryHeader;

        if (reader.Read(static_cast<void*>(&entryHeader), sizeof(entryHeader)) != sizeof(entryHeader)
            || entryHeader.nameStr[0] == '\0'
            || entryHeader.nameStr[sizeof(entryHeader.nameStr) - 1] != '\0'
            || entryHeader.numBindings > 1024)
        {
            return false;
        }

        Entry entry;
        entry.shaderName = CreateNameFromDynamicString(entryHeader.nameStr);
        entry.properties = entryHeader.properties;
        entry.bindings.Resize(entryHeader.numBindings);

        const size_t bindingsSize = entry.bindings.Size() * sizeof(WebGPUPipelineManifestBinding);

        if (bindingsSize != 0 && reader.Read(static_cast<void*>(entry.bindings.Data()), bindingsSize) != bindingsSize)
        {
            return false;
        }

        entry.key = GetEntryKey(entry);

        m_entries.PushBack(std::move(entry));
    }

    return true;
}

void WebGPUPipelineManifest::Save() const
{
    if (m_entries.Empty())
    {
        return;
    }

    WebGPUPipelineManifestHeader header;
    header.float32Filterable = RI.GetDeviceFeatures().float32Filterable ? 1 : 0;

    for (const Entry& entry : m_entries)
    {
        header.entryCount += entry.isStale ? 0 : 1;
    }

    header.propertyIdCount = GetShaderPropertyCount();

    HashCode propertyDictionaryHashCode;
    GetShaderPropertyDictionaryHashCode(header.propertyIdCount, propertyDictionaryHashCode);

    header.propertyDictionaryHash = propertyDictionaryHashCode.Value();

    const FilePath cacheDir = EngineGlobals::GetCacheDirectory();

    if (!cacheDir.Exists())
    {
        cacheDir.MkDir();
    }

    FileByteWriter writer(cacheDir / FileName);

    if (!writer.IsOpen())
    {
        HYP_LOG(RenderingBackend, Warning, "Could not write the pipeline manifest to {}", cacheDir / FileName);

        return;
    }

    writer.Write(&header, sizeof(header));

    for (const Entry& entry : m_entries)
    {
        if (entry.isStale)
        {
            continue;
        }

        const char* nameStr = entry.shaderName.LookupString();
        const size_t nameLength = Memory::StrLen(nameStr);

        WebGPUPipelineManifestEntryHeader entryHeader;
        Memory::Zero(&entryHeader, sizeof(entryHeader));

        AssertDebug(nameLength < sizeof(entryHeader.nameStr));

        Memory::Copy(entryHeader.nameStr, nameStr, MathUtil::Min(nameLength, sizeof(entryHeader.nameStr) - 1));

        entryHeader.properties = entry.properties;
        entryHeader.numBindings = uint32(entry.bindings.Size());

        writer.Write(&entryHeader, sizeof(entryHeader));
        writer.Write(entry.bindings.Data(), entry.bindings.Size() * sizeof(WebGPUPipelineManifestBinding));
    }

    writer.Close();
}

void WebGPUPipelineManifest::ReleasePipelines()
{
    for (Entry& entry : m_entries)
    {
        entry.pipeline.Reset();
    }
}

bool WebGPUPipelineManifest::Prewarm(Entry& entry)
{
    ComputePipeline* pipeline = RI.computePipelineCache->GetOrCreate(entry.shaderName, entry.properties);

    if (pipeline == nullptr || !pipeline->IsCreated())
    {
        return false;
    }

    const WebGPUShaderInstanceRef& shaderInstance = pipeline->GetShader();

    const uint32 numBindGroups = pipeline->GetNumBindGroups();

    WGPUBindGroupLayout layouts[WebGPUCommandBuffer::MaxBindGroups] = {};

    Array<WGPUBindGroupLayoutEntry, WebGPUAllocator> layoutEntries;

    for (uint32 bindIndex = 0; bindIndex < numBindGroups; bindIndex++)
    {
        const WebGPUReflectedGroup& reflectedGroup = shaderInstance->GetReflectedGroup(bindIndex);

        layoutEntries.Clear();

        for (const WebGPUPipelineManifestBinding& binding : entry.bindings)
        {
            if (binding.group != bindIndex)
            {
                continue;
            }

            WGPUBindGroupLayoutEntry layoutEntry;

            // the shader has changed since the manifest was recorded when it no longer declares what was bound to it
            if (layoutEntries.Size() >= reflectedGroup.bindings.Size()
                || reflectedGroup.bindings[layoutEntries.Size()].binding != binding.binding
                || !ToLayoutEntry(binding, layoutEntry))
            {
                return false;
            }

            layoutEntries.PushBack(layoutEntry);
        }

        if (layoutEntries.Size() != reflectedGroup.bindings.Size())
        {
            return false;
        }

        layouts[bindIndex] = layoutEntries.Any()
            ? RI.GetOrCreateBindGroupLayout(layoutEntries.Data(), uint32(layoutEntries.Size()))
            : RI.GetEmptyBindGroupLayout();
    }

    pipeline->GetOrCreateVariant(layouts, numBindGroups);

    entry.pipeline = MakeStrongRef(pipeline);
    entry.lastTouchedFrame = pipeline->lastFrame;

    return true;
}

void WebGPUPipelineManifest::Update()
{
    if (!m_isLoaded)
    {
        m_isLoaded = true;

        Load();
    }

    const uint32 frameCounter = GetFrameCounter();

    // one a frame: creating a pipeline waits for its shader to load
    bool hasPrewarmed = false;
    bool hasPending = false;

    for (Entry& entry : m_entries)
    {
        if (!entry.isPrewarmed)
        {
            hasPending = true;

            if (hasPrewarmed)
            {
                continue;
            }

            hasPrewarmed = true;

            entry.isPrewarmed = true;

            m_isPrewarming = true;
            const bool didPrewarm = Prewarm(entry);
            m_isPrewarming = false;

            entry.isStale = !didPrewarm;

            if (!didPrewarm)
            {
                HYP_LOG(RenderingBackend, Warning, "Pipeline manifest variant of {} no longer fits its shader, it will compile when it is first used", entry.shaderName);
            }

            continue;
        }

        if (!entry.pipeline.IsValid())
        {
            continue;
        }

        // Kept from being discarded as unused until whatever it was compiled for starts using it
        if (entry.pipeline->lastFrame != entry.lastTouchedFrame)
        {
            entry.pipeline.Reset();

            continue;
        }

        entry.pipeline->lastFrame = frameCounter;
        entry.lastTouchedFrame = frameCounter;
    }

    if (!m_hasReported && m_entries.Any() && !hasPending)
    {
        m_hasReported = true;

        uint32 numStale = 0;

        for (const Entry& entry : m_entries)
        {
            numStale += entry.isStale ? 1 : 0;
        }

        HYP_LOG(RenderingBackend, Info, "Pipeline manifest: {} variant(s) are compiling ahead of use, {} no longer fit their shader", m_entries.Size() - numStale, numStale);
    }
}

#pragma endregion WebGPUPipelineManifest

} // namespace Hyperion
