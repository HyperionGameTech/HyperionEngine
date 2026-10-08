/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#include <WebGPUPch.hpp>

#include <Rendering/WebGPU/WebGPUShaderInstance.hpp>
#include <Rendering/WebGPU/WebGPURenderInterface.hpp>
#include <Rendering/WebGPU/WebGPUHelpers.hpp>

#include <Rendering/Shader.hpp>

#include <WebGPUShaderInstance.generated.inl>

namespace Hyperion {

ENGINE_API HYP_DECLARE_LOG_CHANNEL(RenderingBackend);

extern WebGPURenderInterface RI;

#pragma region WGSL reflection

struct WGSLCursor
{
    const char* current;
    const char* end;

    bool StartsWith(const char* text) const
    {
        const size_t length = std::strlen(text);

        return size_t(end - current) >= length && std::memcmp(current, text, length) == 0;
    }

    bool Consume(const char* text)
    {
        if (!StartsWith(text))
        {
            return false;
        }

        current += std::strlen(text);

        return true;
    }

    void SkipSpaces()
    {
        while (current < end && (*current == ' ' || *current == '\t'))
        {
            ++current;
        }
    }

    bool ReadUInt(uint32& outValue)
    {
        if (current >= end || *current < '0' || *current > '9')
        {
            return false;
        }

        outValue = 0;

        while (current < end && *current >= '0' && *current <= '9')
        {
            outValue = outValue * 10 + uint32(*current - '0');
            ++current;
        }

        Consume("u");

        return true;
    }

    void SkipIdentifier()
    {
        while (current < end && (std::isalnum(static_cast<unsigned char>(*current)) || *current == '_'))
        {
            ++current;
        }
    }

    void SkipLine()
    {
        while (current < end && *current != '\n')
        {
            ++current;
        }

        if (current < end)
        {
            ++current;
        }
    }
};

struct WGSLDimensionName
{
    const char* name;
    WGPUTextureViewDimension dimension;
};

static const WGSLDimensionName g_dimensionNames[] = {
    { "cube_array", WGPUTextureViewDimension_CubeArray },
    { "2d_array", WGPUTextureViewDimension_2DArray },
    { "cube", WGPUTextureViewDimension_Cube },
    { "1d", WGPUTextureViewDimension_1D },
    { "2d", WGPUTextureViewDimension_2D },
    { "3d", WGPUTextureViewDimension_3D }
};

struct WGSLStorageFormatName
{
    const char* name;
    WGPUTextureFormat format;
};

static const WGSLStorageFormatName g_storageFormatNames[] = {
    { "rgba32float", WGPUTextureFormat_RGBA32Float },
    { "rgba32uint", WGPUTextureFormat_RGBA32Uint },
    { "rgba32sint", WGPUTextureFormat_RGBA32Sint },
    { "rgba16float", WGPUTextureFormat_RGBA16Float },
    { "rgba16uint", WGPUTextureFormat_RGBA16Uint },
    { "rgba16sint", WGPUTextureFormat_RGBA16Sint },
    { "rgba16unorm", WGPUTextureFormat_RGBA16Unorm },
    { "rgba8unorm", WGPUTextureFormat_RGBA8Unorm },
    { "rgba8snorm", WGPUTextureFormat_RGBA8Snorm },
    { "rgba8uint", WGPUTextureFormat_RGBA8Uint },
    { "rgba8sint", WGPUTextureFormat_RGBA8Sint },
    { "bgra8unorm", WGPUTextureFormat_BGRA8Unorm },
    { "rgb10a2unorm", WGPUTextureFormat_RGB10A2Unorm },
    { "rg11b10ufloat", WGPUTextureFormat_RG11B10Ufloat },
    { "rg32float", WGPUTextureFormat_RG32Float },
    { "rg32uint", WGPUTextureFormat_RG32Uint },
    { "rg32sint", WGPUTextureFormat_RG32Sint },
    { "rg16float", WGPUTextureFormat_RG16Float },
    { "rg16uint", WGPUTextureFormat_RG16Uint },
    { "rg16sint", WGPUTextureFormat_RG16Sint },
    { "rg8unorm", WGPUTextureFormat_RG8Unorm },
    { "rg8uint", WGPUTextureFormat_RG8Uint },
    { "r32float", WGPUTextureFormat_R32Float },
    { "r32uint", WGPUTextureFormat_R32Uint },
    { "r32sint", WGPUTextureFormat_R32Sint },
    { "r16float", WGPUTextureFormat_R16Float },
    { "r16uint", WGPUTextureFormat_R16Uint },
    { "r16sint", WGPUTextureFormat_R16Sint },
    { "r16unorm", WGPUTextureFormat_R16Unorm },
    { "rg16unorm", WGPUTextureFormat_RG16Unorm },
    { "r8unorm", WGPUTextureFormat_R8Unorm },
    { "r8snorm", WGPUTextureFormat_R8Snorm },
    { "r8uint", WGPUTextureFormat_R8Uint },
    { "r8sint", WGPUTextureFormat_R8Sint },
    { "rg8snorm", WGPUTextureFormat_RG8Snorm },
    { "rg8sint", WGPUTextureFormat_RG8Sint },
    { "rgb10a2uint", WGPUTextureFormat_RGB10A2Uint }
};

static bool ReadDimension(WGSLCursor& cursor, WGPUTextureViewDimension& outDimension)
{
    for (const WGSLDimensionName& dimensionName : g_dimensionNames)
    {
        if (cursor.Consume(dimensionName.name))
        {
            outDimension = dimensionName.dimension;

            return true;
        }
    }

    return false;
}

static bool ReadResourceType(WGSLCursor& cursor, bool isUniform, bool isStorage, bool isWritableStorage, WebGPUReflectedBinding& outBinding)
{
    if (isUniform)
    {
        outBinding.kind = WebGPUBindingKind::UniformBuffer;

        return true;
    }

    if (isStorage)
    {
        outBinding.kind = isWritableStorage ? WebGPUBindingKind::StorageBuffer : WebGPUBindingKind::ReadOnlyStorageBuffer;

        return true;
    }

    if (cursor.Consume("sampler_comparison"))
    {
        outBinding.kind = WebGPUBindingKind::ComparisonSampler;

        return true;
    }

    if (cursor.Consume("sampler"))
    {
        outBinding.kind = WebGPUBindingKind::Sampler;

        return true;
    }

    if (cursor.Consume("texture_storage_"))
    {
        outBinding.kind = WebGPUBindingKind::StorageTexture;

        if (!ReadDimension(cursor, outBinding.viewDimension) || !cursor.Consume("<"))
        {
            return false;
        }

        bool foundFormat = false;

        for (const WGSLStorageFormatName& formatName : g_storageFormatNames)
        {
            if (cursor.Consume(formatName.name))
            {
                outBinding.storageFormat = formatName.format;
                foundFormat = true;

                break;
            }
        }

        if (!foundFormat || !cursor.Consume(","))
        {
            return false;
        }

        cursor.SkipSpaces();

        if (cursor.Consume("read_write"))
        {
            outBinding.storageAccess = WGPUStorageTextureAccess_ReadWrite;
        }
        else if (cursor.Consume("read"))
        {
            outBinding.storageAccess = WGPUStorageTextureAccess_ReadOnly;
        }
        else if (cursor.Consume("write"))
        {
            outBinding.storageAccess = WGPUStorageTextureAccess_WriteOnly;
        }
        else
        {
            return false;
        }

        return true;
    }

    if (cursor.Consume("texture_depth_"))
    {
        outBinding.kind = WebGPUBindingKind::DepthTexture;
        outBinding.sampleType = WGPUTextureSampleType_Depth;

        return ReadDimension(cursor, outBinding.viewDimension);
    }

    if (cursor.Consume("texture_"))
    {
        outBinding.kind = WebGPUBindingKind::Texture;

        if (!ReadDimension(cursor, outBinding.viewDimension) || !cursor.Consume("<"))
        {
            return false;
        }

        if (cursor.Consume("f32"))
        {
            outBinding.sampleType = WGPUTextureSampleType_Float;
        }
        else if (cursor.Consume("u32"))
        {
            outBinding.sampleType = WGPUTextureSampleType_Uint;
        }
        else if (cursor.Consume("i32"))
        {
            outBinding.sampleType = WGPUTextureSampleType_Sint;
        }
        else
        {
            return false;
        }

        return true;
    }

    return false;
}

static WGPUShaderStage ToWGPUShaderStage(ShaderModuleType moduleType)
{
    switch (moduleType)
    {
    case ShaderModuleType::Vertex:
        return WGPUShaderStage_Vertex;
    case ShaderModuleType::Pixel:
        return WGPUShaderStage_Fragment;
    case ShaderModuleType::Compute:
        return WGPUShaderStage_Compute;
    default:
        return WGPUShaderStage_None;
    }
}

static const char* GetStorageFormatName(WGPUTextureFormat format)
{
    for (const WGSLStorageFormatName& formatName : g_storageFormatNames)
    {
        if (formatName.format == format)
        {
            return formatName.name;
        }
    }

    return nullptr;
}

static bool ContainsText(const char* source, size_t length, const char* text)
{
    const size_t textLength = std::strlen(text);

    if (textLength == 0 || textLength > length)
    {
        return false;
    }

    const char* end = source + length - textLength;

    for (const char* it = source; it <= end; ++it)
    {
        if (*it == text[0] && std::memcmp(it, text, textLength) == 0)
        {
            return true;
        }
    }

    return false;
}

bool WebGPUShaderInstance::Reflect(ShaderModuleType moduleType, const char* source, size_t length)
{
    const WGPUShaderStage stage = ToWGPUShaderStage(moduleType);

    bool hasStorageTextures = false;

    WGSLCursor cursor { source, source + length };

    // Tint writes each resource as one line: @group(0u) @binding(3u) var<uniform> Name : Type;
    while (cursor.current < cursor.end)
    {
        if (!cursor.Consume("@group("))
        {
            cursor.SkipLine();

            continue;
        }

        WebGPUReflectedBinding reflectedBinding;
        reflectedBinding.visibility = stage;

        bool isUniform = false;
        bool isStorage = false;
        bool isWritableStorage = false;

        bool isParsed = cursor.ReadUInt(reflectedBinding.group) && cursor.Consume(")");

        cursor.SkipSpaces();

        isParsed = isParsed && cursor.Consume("@binding(") && cursor.ReadUInt(reflectedBinding.binding) && cursor.Consume(")");

        cursor.SkipSpaces();

        isParsed = isParsed && cursor.Consume("var");

        if (isParsed && cursor.Consume("<"))
        {
            if (cursor.Consume("uniform"))
            {
                isUniform = true;
            }
            else if (cursor.Consume("storage"))
            {
                isStorage = true;

                if (cursor.Consume(","))
                {
                    cursor.SkipSpaces();

                    isWritableStorage = cursor.StartsWith("read_write");
                }
            }

            while (cursor.current < cursor.end && *cursor.current != '>' && *cursor.current != '\n')
            {
                ++cursor.current;
            }

            isParsed = cursor.Consume(">");
        }

        const char* nameBegin = cursor.current;
        const char* nameEnd = cursor.current;

        if (isParsed)
        {
            cursor.SkipSpaces();

            nameBegin = cursor.current;
            cursor.SkipIdentifier();
            nameEnd = cursor.current;

            cursor.SkipSpaces();

            isParsed = cursor.Consume(":");

            cursor.SkipSpaces();
        }

        isParsed = isParsed && ReadResourceType(cursor, isUniform, isStorage, isWritableStorage, reflectedBinding);

        cursor.SkipLine();

        if (isParsed && reflectedBinding.kind == WebGPUBindingKind::StorageTexture)
        {
            hasStorageTextures = true;

            ANSIString loadCall = "textureLoad(";
            loadCall += ANSIStringView(nameBegin, nameEnd);
            loadCall += ",";

            reflectedBinding.isStorageTextureRead = ContainsText(source, length, loadCall.Data());
        }

        if (!isParsed)
        {
            HYP_LOG(RenderingBackend, Error, "Could not read a resource declaration in the WGSL of shader {}", m_shader->GetName());

            continue;
        }

        if (reflectedBinding.group >= WebGPUCommandBuffer::MaxBindGroups)
        {
            HYP_LOG(RenderingBackend, Error, "Shader {} uses bind group {}, WebGPU has {}", m_shader->GetName(), reflectedBinding.group, WebGPUCommandBuffer::MaxBindGroups);

            continue;
        }

        WebGPUReflectedGroup& reflectedGroup = m_reflectedGroups[reflectedBinding.group];

        bool isMerged = false;

        for (WebGPUReflectedBinding& existingBinding : reflectedGroup.bindings)
        {
            if (existingBinding.binding == reflectedBinding.binding)
            {
                existingBinding.visibility |= stage;
                existingBinding.isStorageTextureRead |= reflectedBinding.isStorageTextureRead;
                isMerged = true;

                break;
            }
        }

        if (!isMerged)
        {
            reflectedGroup.bindings.PushBack(reflectedBinding);
        }

        m_numBindGroups = MathUtil::Max(m_numBindGroups, reflectedBinding.group + 1);
    }

    return hasStorageTextures;
}

static void ReadOutputLocations(WGSLCursor cursor, const char* terminator, uint32& outMask)
{
    while (cursor.current < cursor.end && !cursor.StartsWith(terminator))
    {
        uint32 location = 0;

        if (cursor.Consume("@location(") && cursor.ReadUInt(location))
        {
            if (location < 32)
            {
                outMask |= 1u << location;
            }

            continue;
        }

        ++cursor.current;
    }
}

void WebGPUShaderInstance::ReflectFragmentOutputs(ANSIStringView entryPointName, const char* source, size_t length)
{
    ANSIString signature = "fn ";
    signature += entryPointName;
    signature += "(";

    WGSLCursor cursor { source, source + length };

    while (cursor.current < cursor.end && !cursor.StartsWith(signature.Data()))
    {
        cursor.SkipLine();
    }

    if (cursor.current >= cursor.end)
    {
        return;
    }

    const char* lineEnd = cursor.current;

    while (lineEnd < cursor.end && *lineEnd != '\n')
    {
        ++lineEnd;
    }

    const char* returnType = nullptr;

    for (const char* it = cursor.current; it + 1 < lineEnd; ++it)
    {
        if (it[0] == '-' && it[1] == '>')
        {
            returnType = it + 2;
        }
    }

    if (returnType == nullptr)
    {
        return;
    }

    WGSLCursor returnCursor { returnType, lineEnd };
    returnCursor.SkipSpaces();

    if (returnCursor.StartsWith("@"))
    {
        ReadOutputLocations(returnCursor, "{", m_fragmentOutputMask);

        return;
    }

    const char* structNameBegin = returnCursor.current;
    returnCursor.SkipIdentifier();

    ANSIString structDeclaration = "struct ";
    structDeclaration += ANSIStringView(structNameBegin, returnCursor.current);
    structDeclaration += " {";

    WGSLCursor structCursor { source, source + length };

    while (structCursor.current < structCursor.end && !structCursor.StartsWith(structDeclaration.Data()))
    {
        structCursor.SkipLine();
    }

    ReadOutputLocations(structCursor, "}", m_fragmentOutputMask);
}

#pragma endregion WGSL reflection

WebGPUShaderInstance::WebGPUShaderInstance()
    : ShaderInstanceBase(),
      m_numBindGroups(0),
      m_fragmentOutputMask(0)
{
}

WebGPUShaderInstance::WebGPUShaderInstance(const Shader* shader)
    : ShaderInstanceBase(shader),
      m_numBindGroups(0),
      m_fragmentOutputMask(0)
{
#ifdef HYP_RHI_DEBUG_NAMES
    if (shader != nullptr)
    {
        SetDebugName(shader->baseName);
    }
#endif
}

WebGPUShaderInstance::~WebGPUShaderInstance()
{
    for (ShaderModule& shaderModule : m_shaderModules)
    {
        for (ShaderModuleVariant& variant : shaderModule.variants)
        {
            if (variant.shaderModule != nullptr)
            {
                wgpuShaderModuleRelease(variant.shaderModule);
            }
        }

        if (shaderModule.shaderModule != nullptr)
        {
            wgpuShaderModuleRelease(shaderModule.shaderModule);
        }
    }
}

WGPUShaderModule WebGPUShaderInstance::GetShaderModuleVariant(ShaderModuleType type, Span<const WebGPUStorageTextureOverride> overrides)
{
    ShaderModule* shaderModule = nullptr;

    for (ShaderModule& it : m_shaderModules)
    {
        if (it.type == type)
        {
            shaderModule = &it;

            break;
        }
    }

    if (shaderModule == nullptr)
    {
        return nullptr;
    }

    if (overrides.Size() == 0 || shaderModule->source.Empty())
    {
        return shaderModule->shaderModule;
    }

    HashCode hashCode;

    for (const WebGPUStorageTextureOverride& storageOverride : overrides)
    {
        hashCode.Add(storageOverride.group);
        hashCode.Add(storageOverride.binding);
        hashCode.Add(uint32(storageOverride.format));
        hashCode.Add(uint32(storageOverride.access));
    }

    const uint64 key = hashCode.Value();

    for (const ShaderModuleVariant& variant : shaderModule->variants)
    {
        if (variant.key == key)
        {
            return variant.shaderModule;
        }
    }

    const char* source = shaderModule->source.Data();
    const size_t sourceLength = shaderModule->source.Size();

    ANSIString patchedSource;

    WGSLCursor cursor { source, source + sourceLength };

    while (cursor.current < cursor.end)
    {
        const char* lineBegin = cursor.current;

        WGSLCursor lineCursor = cursor;
        cursor.SkipLine();

        const char* lineEnd = cursor.current;

        uint32 group = 0;
        uint32 binding = 0;

        const WebGPUStorageTextureOverride* matchingOverride = nullptr;

        if (lineCursor.Consume("@group(") && lineCursor.ReadUInt(group) && lineCursor.Consume(")"))
        {
            lineCursor.SkipSpaces();

            if (lineCursor.Consume("@binding(") && lineCursor.ReadUInt(binding))
            {
                for (const WebGPUStorageTextureOverride& storageOverride : overrides)
                {
                    if (storageOverride.group == group && storageOverride.binding == binding)
                    {
                        matchingOverride = &storageOverride;

                        break;
                    }
                }
            }
        }

        const char* formatName = matchingOverride != nullptr ? GetStorageFormatName(matchingOverride->format) : nullptr;

        const char* typeArgumentsBegin = nullptr;
        const char* typeArgumentsEnd = nullptr;

        if (formatName != nullptr)
        {
            // the last <...> on the line is the one after texture_storage_*, the first belongs to var<...> when present
            for (const char* it = lineBegin; it < lineEnd; ++it)
            {
                if (*it == '<')
                {
                    typeArgumentsBegin = it + 1;
                }
                else if (*it == '>')
                {
                    typeArgumentsEnd = it;
                }
            }
        }

        if (typeArgumentsBegin == nullptr || typeArgumentsEnd == nullptr || typeArgumentsEnd < typeArgumentsBegin)
        {
            patchedSource += ANSIStringView(lineBegin, lineEnd);

            continue;
        }

        const char* accessName = "write";

        if (matchingOverride->access == WGPUStorageTextureAccess_ReadWrite)
        {
            accessName = "read_write";
        }
        else if (matchingOverride->access == WGPUStorageTextureAccess_ReadOnly)
        {
            accessName = "read";
        }

        patchedSource += ANSIStringView(lineBegin, typeArgumentsBegin);
        patchedSource += formatName;
        patchedSource += ", ";
        patchedSource += accessName;
        patchedSource += ANSIStringView(typeArgumentsEnd, lineEnd);
    }

    WGPUShaderSourceWGSL shaderSource = WGPU_SHADER_SOURCE_WGSL_INIT;
    shaderSource.code = ToWGPUStringView(patchedSource.Data(), patchedSource.Size());

    WGPUShaderModuleDescriptor descriptor = WGPU_SHADER_MODULE_DESCRIPTOR_INIT;
    descriptor.nextInChain = &shaderSource.chain;

    WGPUShaderModule variantModule = wgpuDeviceCreateShaderModule(RI.GetDevice(), &descriptor);

    shaderModule->variants.PushBack(ShaderModuleVariant { key, variantModule });

    return variantModule;
}

WGPUShaderModule WebGPUShaderInstance::GetShaderModule(ShaderModuleType type) const
{
    for (const ShaderModule& shaderModule : m_shaderModules)
    {
        if (shaderModule.type == type)
        {
            return shaderModule.shaderModule;
        }
    }

    return nullptr;
}

ANSIStringView WebGPUShaderInstance::GetEntryPointName(ShaderModuleType type) const
{
    for (const ShaderModule& shaderModule : m_shaderModules)
    {
        if (shaderModule.type == type)
        {
            return shaderModule.entryPointName;
        }
    }

    return ANSIStringView();
}

bool WebGPUShaderInstance::IsCreated() const
{
    return m_shaderModules.Any();
}

RendererResult WebGPUShaderInstance::Create()
{
    if (IsCreated())
    {
        return {};
    }

    if (!m_shader || !m_shader->IsValid())
    {
        return HYP_MAKE_ERROR(RendererError, "Invalid Shader, cannot create ShaderInstance!");
    }

    auto readScope = m_shader->GetReadScope();

    for (size_t index = 0; index < m_shader->moduleTypes.Size(); index++)
    {
        ShaderModuleType moduleType;
        String moduleName;
        String entryPointName;
        ConstByteView blob;

        if (!m_shader->GetShaderModuleInfo(uint32(index), moduleType, moduleName, entryPointName, blob))
        {
            continue;
        }

        if (blob.Size() == 0)
        {
            continue;
        }

        if (blob.Data() == nullptr)
        {
            return HYP_MAKE_ERROR(RendererError, "Blob data for shader '{}' module '{}' failed to page in, the shader cache may be stale", 0, m_shader->GetName(), moduleName);
        }

        const char* source = reinterpret_cast<const char*>(blob.Data());
        size_t sourceLength = blob.Size();

        while (sourceLength > 0 && source[sourceLength - 1] == '\0')
        {
            --sourceLength;
        }

        WGPUShaderSourceWGSL shaderSource = WGPU_SHADER_SOURCE_WGSL_INIT;
        shaderSource.code = ToWGPUStringView(source, sourceLength);

        WGPUShaderModuleDescriptor descriptor = WGPU_SHADER_MODULE_DESCRIPTOR_INIT;
        descriptor.nextInChain = &shaderSource.chain;
        descriptor.label = ToWGPUStringView(moduleName.Data(), moduleName.Size());

        WGPUShaderModule wgpuShaderModule = wgpuDeviceCreateShaderModule(RI.GetDevice(), &descriptor);

        if (wgpuShaderModule == nullptr)
        {
            return HYP_MAKE_ERROR(RendererError, "Failed to create WebGPU shader module '{}' for shader '{}'", 0, moduleName, m_shader->GetName());
        }

        ShaderModule& shaderModule = m_shaderModules.EmplaceBack();
        shaderModule.type = moduleType;
        shaderModule.shaderModule = wgpuShaderModule;
        shaderModule.entryPointName = ANSIString(entryPointName.Data());

        if (Reflect(moduleType, source, sourceLength))
        {
            shaderModule.source = ANSIString(ANSIStringView(source, source + sourceLength));
        }

        if (moduleType == ShaderModuleType::Pixel)
        {
            ReflectFragmentOutputs(shaderModule.entryPointName, source, sourceLength);
        }
    }

    for (WebGPUReflectedGroup& reflectedGroup : m_reflectedGroups)
    {
        std::sort(reflectedGroup.bindings.Begin(), reflectedGroup.bindings.End(), [](const WebGPUReflectedBinding& a, const WebGPUReflectedBinding& b)
            {
                return a.binding < b.binding;
            });

        HashCode hashCode;

        for (const WebGPUReflectedBinding& reflectedBinding : reflectedGroup.bindings)
        {
            hashCode.Add(reflectedBinding.binding);
            hashCode.Add(uint32(reflectedBinding.kind));
            hashCode.Add(uint32(reflectedBinding.viewDimension));
            hashCode.Add(uint32(reflectedBinding.sampleType));
            hashCode.Add(uint32(reflectedBinding.storageFormat));
            hashCode.Add(uint32(reflectedBinding.storageAccess));
            hashCode.Add(uint64(reflectedBinding.visibility));
        }

        reflectedGroup.hashCode = hashCode.Value();
    }

    return {};
}

#ifdef HYP_RHI_DEBUG_NAMES
void WebGPUShaderInstance::SetDebugName(Name name)
{
    ShaderInstanceBase::SetDebugName(name);
}
#endif

} // namespace Hyperion
