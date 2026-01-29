/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <RenderingPch.hpp>

#include <rendering/util/ShaderCompiler.hpp>
#include <rendering/util/ShaderPropertyCache.hpp>

#include <core/filesystem/FsUtil.hpp>

#include <core/json/JSON.hpp>

#include <core/utilities/ByteUtil.hpp>
#include <core/utilities/ForEach.hpp>
#include <core/utilities/Time.hpp>

#include <core/functional/Proc.hpp>

#include <core/io/ByteWriter.hpp>
#include <core/io/BufferedByteReader.hpp>

#include <util/ini/INIFile.hpp>

#include <core/serialization/fbom/FBOMReader.hpp>
#include <core/serialization/fbom/FBOMWriter.hpp>

#include <core/math/MathUtil.hpp>

#include <engine/EngineDriver.hpp>

#include <rendering/RenderInterface.hpp>
#include <rendering/RenderConfig.hpp>
#include <rendering/DescriptorSet.hpp>
#include <rendering/Shader.hpp>

#define HYP_SHADER_REFLECTION 1

#if HYP_GLSLANG
#include <glslang/Include/ResourceLimits.h>
#include <glslang/Include/Types.h>
#include <glslang/Include/glslang_c_interface.h>
#include <glslang/Public/ShaderLang.h>

#if HYP_SHADER_REFLECTION
#include <glslang/MachineIndependent/reflection.h>
#endif
#endif

#if HYP_DXC
#include <Unknwn.h>
#include <dxcapi.h>
#include <d3d12shader.h>
#include <wrl/client.h>
#endif

#if HYP_VULKAN
#include <vulkan/vulkan.h>
#endif

#include <HyperionEngine.hpp>

#include <ShaderCompiler.generated.inl>

#if HYP_DXC
// {5A58797D-A72C-478D-8BA2-EFC6B0EFE88E}
//interface DECLSPEC_UUID("5A58797D-A72C-478D-8BA2-EFC6B0EFE88E") ID3D12ShaderReflection;
DEFINE_GUID(IID_ID3D12ShaderReflection, 0x5a58797d, 0xa72c, 0x478d, 0x8b, 0xa2, 0xef, 0xc6, 0xb0, 0xef, 0xe8, 0x8e);
#endif

namespace Hyperion {

HYP_DEFINE_LOG_SUBCHANNEL(ShaderCompiler, Core);

static constexpr bool ShouldCompileMissingVariants = false;
static constexpr bool ShouldCompileEntireBundle = false; // aggressively compile all permutations defined

namespace CoreApi {
extern const GlobalConfig& GetGlobalConfig();
} // namespace CoreApi

// #define HYP_SHADER_COMPILER_LOGGING

#if HYP_DXC
using Microsoft::WRL::ComPtr;

static ComPtr<IDxcUtils> s_dxcUtils;
static ComPtr<IDxcCompiler3> s_dxcCompiler;
#endif

#pragma region Helpers

#if HYP_DXC
static LPCWSTR GetDXCTargetProfile(ShaderModuleType type)
{
    switch (type)
    {
    case ShaderModuleType::Vertex:
        return L"vs_6_1"; // need 6.1 for multiple views
    case ShaderModuleType::Pixel:
        return L"ps_6_0";
    case ShaderModuleType::Geometry:
        return L"gs_6_0";
    case ShaderModuleType::Compute:
        return L"cs_6_0";
    case ShaderModuleType::RayGen:
    case ShaderModuleType::Miss:
    case ShaderModuleType::ClosestHit:
    case ShaderModuleType::AnyHit:
    case ShaderModuleType::Intersect:
        return L"lib_6_3";
    default:
        return L"vs_6_0";
    }
}
#endif

String GetShaderVersionFromSource(const String& source, String& outSourceWithoutVersion)
{
    outSourceWithoutVersion = source;

    String sourceTrimmed = source.TrimmedLeft();

    if (sourceTrimmed.StartsWith("#version"))
    {
        SizeType firstNewline = sourceTrimmed.FindFirstIndex('\n');
        String versionLine = sourceTrimmed.Substr(0, firstNewline);

        outSourceWithoutVersion = sourceTrimmed.Substr(firstNewline + 1);

        return versionLine.TrimmedRight();
    }

    return "#version 450";
}

static String BuildDescriptorTableDefines(ShaderLanguage language, const ShaderInputGroup& inputGroup)
{
    String descriptorTableDefines;

    // Generate descriptor table defines
    for (const DescriptorSetDeclaration& descriptorSetDeclaration : inputGroup.elements)
    {
        const DescriptorSetDeclaration* descriptorSetDeclarationPtr = &descriptorSetDeclaration;

        const uint32 setIndex = inputGroup.GetDescriptorSetIndex(descriptorSetDeclaration.name);
        Assert(setIndex != -1);

        if (language == ShaderLanguage::GLSL)
        {
            descriptorTableDefines += "#define _" + String(*descriptorSetDeclaration.name) + "_SET" + " " + String::ToString(setIndex) + "\n";
        }
        else if (language == ShaderLanguage::HLSL)
        {
            descriptorTableDefines += "#define _" + String(*descriptorSetDeclaration.name) + "_SPACE" + " " + ("space" + String::ToString(setIndex)) + "\n";
        }

        if (descriptorSetDeclaration.flags[DescriptorSetDeclarationFlags::REFERENCE])
        {
            const DescriptorSetDeclaration* referencedDescriptorSetDeclaration = GetStaticDescriptorTableDeclaration().FindDescriptorSetDeclaration(descriptorSetDeclaration.name);
            Assert(referencedDescriptorSetDeclaration != nullptr);

            descriptorSetDeclarationPtr = referencedDescriptorSetDeclaration;
        }

        for (const Array<ShaderInput>& descriptorDeclarations : descriptorSetDeclarationPtr->slots)
        {
            for (const ShaderInput& shaderInput : descriptorDeclarations)
            {
                descriptorTableDefines += '\t';

                switch (language)
                {
                case ShaderLanguage::GLSL:
                {
                    const uint32 flatIndex = descriptorSetDeclarationPtr->CalculateFlatIndex(shaderInput.slot, shaderInput.name);
                    Assert(flatIndex != uint32(-1));

                    descriptorTableDefines += HYP_FORMAT("#define _{}_{}_BINDING {}",
                        descriptorSetDeclarationPtr->name, shaderInput.name,
                        flatIndex);

                    break;
                }
                case ShaderLanguage::HLSL:
                {
                    char registerKey = 0;

                    switch (shaderInput.slot)
                    {
                    case ShaderRegister::SRV: // read-only storage buffers and textures
                        registerKey = 't';
                        break;
                    case ShaderRegister::UAV: // r/w storage buffers and images
                        registerKey = 'u';
                        break;
                    case ShaderRegister::BUFFER: // constant buffers
                        registerKey = 'b';
                        break;
                    case ShaderRegister::SAMPLER: // samplers
                        registerKey = 's';
                        break;
                    default:
                        HYP_UNREACHABLE();
                    }

#if HYP_VULKAN
                    const uint32 registerIndex = descriptorSetDeclarationPtr->CalculateFlatIndex(shaderInput.slot, shaderInput.name);
#elif HYP_DX12
                    const uint32 registerIndex = shaderInput.index;
#endif

                    descriptorTableDefines += HYP_FORMAT("#define _{}_{}_REGISTER {}{}",
                        descriptorSetDeclarationPtr->name, shaderInput.name,
                        registerKey, registerIndex);

                    break;
                }
                default:
                    HYP_UNREACHABLE();
                }

                descriptorTableDefines += '\n';
            }
        }
    }

    return descriptorTableDefines;
}

static String BuildAttributesDefines(
    ShaderLanguage language,
    const ShaderVariantPerms& perm)
{
    String preamble;

    for (const VertexAttribute* attr : perm.GetRequiredVertexAttributes().BuildAttributes())
    {
        preamble += String("#define HYP_ATTRIBUTE_") + attr->name + "\n";
    }

    // We do not do the same for Optional attributes, as they have not been
    // instantiated at this point in time. before compiling the shader, they
    // should have all been made Required.

    HashSet<StringHash> definedNames;

    for (const ShaderProperty& property : perm.GetPropertySet())
    {
        if (!property.name.IsValid())
        {
            continue;
        }

        if (definedNames.Contains(StringHash(property.name)))
        {
            HYP_LOG(ShaderCompiler,
                Warning,
                "Shader property {} defined multiple times in shader properties! This may cause shader compilation errors.",
                property.name);

            HYP_BREAKPOINT_DEBUG_MODE;

            continue;
        }

        definedNames.Insert(StringHash(property.name));

        // property has a value -- if integral or float, use that value
        if (property.HasValue())
        {
            if (property.currentValue.Is<Name>())
            {
                // string values are defined as KEY_VALUE = 1
                preamble += HYP_FORMAT("#define {}_{} 1\n", property.name, property.currentValue.Get<Name>());
            }
            else
            {
                preamble += HYP_FORMAT("#define {} {}\n", property.name, property.GetValueString());
            }

            continue;
        }

        // no value set, treat it as boolean true (1)
        preamble += HYP_FORMAT("#define {} 1\n", property.name);
    }

    return preamble;
}

void MergeGlobalShaderProperties(ShaderVariantPerms& inOutPerm)
{
    static const GlobalConfig& s_globalConfig = CoreApi::GetGlobalConfig();

#if HYP_VULKAN
    constexpr int VulkanVersion = HYP_VULKAN_API_VERSION;
    inOutPerm.Set(ShaderProperty(NAME("HYP_VULKAN"), VulkanVersion));
#endif

#if defined(HYP_WINDOWS)
    inOutPerm.Set(ShaderProperty(NAME("HYP_WINDOWS")));
#elif defined(HYP_LINUX)
    inOutPerm.Set(ShaderProperty(NAME("HYP_LINUX")));
#elif defined(HYP_MACOS)
    inOutPerm.Set(ShaderProperty(NAME("HYP_MACOS")));
#elif defined(HYP_IOS)
    inOutPerm.Set(ShaderProperty(NAME("HYP_IOS")));
#endif

    inOutPerm.Set(ShaderProperty(NAME("NUM_GBUFFER_TEXTURES"), ShaderProperty::Value(int(NumGBufferTargets))));

    if (g_renderInterface->GetRenderConfig().bindlessTextures)
        inOutPerm.Set(ShaderProperty(NAME("HYP_FEATURES_BINDLESS_TEXTURES")));

    if (s_globalConfig.Get("Rendering.Debug.Reflections").ToBool(false))
        inOutPerm.Set(ShaderProperty(NAME("DEBUG_REFLECTIONS")));

    if (s_globalConfig.Get("Rendering.Debug.Irradiance").ToBool(false))
        inOutPerm.Set(ShaderProperty(NAME("DEBUG_IRRADIANCE")));

    if (s_globalConfig.Get("Rendering.Debug.Velocity").ToBool(false))
        inOutPerm.Set(ShaderProperty(NAME("DEBUG_VELOCITY")));

    if (s_globalConfig.Get("Rendering.Debug.Normals").ToBool(false))
        inOutPerm.Set(ShaderProperty(NAME("DEBUG_NORMALS")));

    // inOutPerm.Set(ShaderProperty("HYP_MAX_SHADOW_MAPS"));
    // inOutPerm.Set(ShaderProperty("HYP_MAX_BONES"));
}

void MergeGlobalShaderProperties(ShaderPropertySet& out)
{
    ShaderVariantPerms perm;
    MergeGlobalShaderProperties(perm);

    for (const ShaderProperty& property : perm.GetPropertySet())
    {
        const ShaderPropertyId propertyId = InternShaderProperty(property);

        out.Add(propertyId);
    }
}

static bool SatisfiesRequested(
    const ShaderPropertySet& requestedProperties, const VertexAttributeSet& requestedVertexAttributes,
    const CompiledShader& candidate)
{
    if (candidate.properties != requestedProperties)
    {
        return false;
    }

    if (requestedVertexAttributes == 0)
    {
        return true;
    }

    // Satisfies if:
    //  candidate can has the attributes we requested for (AND) candidate does not require any attributes that we don't have.
    if (candidate.vertexAttributes == requestedVertexAttributes)
    {
        return true;
    }

    return false;
}

#pragma endregion Helpers

#pragma region Internal structures



enum class DescriptorUsageFlags : uint32
{
    NONE = 0x0,
    DYNAMIC = 0x1
};

HYP_MAKE_ENUM_FLAGS(DescriptorUsageFlags)

struct StructureType
{
    Name name;
    uint32 size = ~0u;
    Array<Name> fieldNames;
    Array<StructureType, DynamicAllocator> fieldTypes;

    StructureType() = default;

    StructureType(Name name, uint32 size = ~0u)
        : name(name),
          size(size)
    {
    }

    StructureType(const StructureType& other) = default;
    StructureType& operator=(const StructureType& other) = default;
    StructureType(StructureType&& other) noexcept = default;
    StructureType& operator=(StructureType&& other) noexcept = default;

    HYP_FORCE_INLINE bool IsValid() const
    {
        return name.IsValid();
    }

    HYP_FORCE_INLINE bool HasExplicitSize() const
    {
        return size != ~0u;
    }

    HYP_FORCE_INLINE Name GetName() const
    {
        return name;
    }

    HYP_FORCE_INLINE uint32 GetSize() const
    {
        return size;
    }

    HYP_FORCE_INLINE Pair<Name, StructureType&> AddField(Name fieldName, const StructureType& type)
    {
        return Pair<Name, StructureType&> { fieldNames.PushBack(fieldName), fieldTypes.PushBack(type) };
    }

    HYP_FORCE_INLINE Pair<Name, StructureType&> GetField(SizeType index)
    {
        return { fieldNames[index], fieldTypes[index] };
    }

    HYP_FORCE_INLINE const Pair<Name, const StructureType&> GetField(SizeType index) const
    {
        return { fieldNames[index], fieldTypes[index] };
    }

    HYP_FORCE_INLINE Optional<Pair<Name, StructureType&>> FindField(StringHash fieldName)
    {
        for (SizeType i = 0; i < fieldNames.Size(); i++)
        {
            if (fieldNames[i] == fieldName)
            {
                return Pair<Name, StructureType&> { fieldNames[i], fieldTypes[i] };
            }
        }

        return {};
    }

    HYP_FORCE_INLINE Optional<Pair<Name, const StructureType&>> FindField(StringHash fieldName) const
    {
        for (SizeType i = 0; i < fieldNames.Size(); i++)
        {
            if (fieldNames[i] == fieldName)
            {
                return Pair<Name, const StructureType&> { fieldNames[i], fieldTypes[i] };
            }
        }

        return {};
    }

    HYP_FORCE_INLINE bool operator<(const StructureType& other) const
    {
        if (size != other.size)
        {
            return size < other.size;
        }

        if (fieldTypes.Size() != other.fieldTypes.Size())
        {
            return fieldTypes.Size() < other.fieldTypes.Size();
        }

        for (SizeType i = 0; i < fieldTypes.Size(); i++)
        {
            if (fieldTypes[i] != other.fieldTypes[i])
            {
                return fieldTypes[i] < other.fieldTypes[i];
            }
        }

        return false;
    }

    HYP_FORCE_INLINE bool operator==(const StructureType& other) const
    {
        return name == other.name
            && size == other.size
            && fieldNames == other.fieldNames
            && fieldTypes == other.fieldTypes;
    }

    HYP_FORCE_INLINE bool operator!=(const StructureType& other) const
    {
        return name != other.name
            || size != other.size
            || fieldNames != other.fieldNames
            || fieldTypes != other.fieldTypes;
    }

    HYP_FORCE_INLINE HashCode GetHashCode() const
    {
        HashCode hc;
        hc.Add(name);
        hc.Add(size);
        hc.Add(fieldNames);
        hc.Add(fieldTypes);

        return hc;
    }
};

struct DescriptorUsage
{
    ShaderRegister slot;
    ShaderInputType type;
    Name setName;
    Name descriptorName;
    StructureType structureType;
    EnumFlags<DescriptorUsageFlags> flags;
    HashMap<String, String> params;

    DescriptorUsage()
        : slot(ShaderRegister::NONE),
          type(ShaderInputType::UNSET),
          setName(Name::Invalid()),
          flags(DescriptorUsageFlags::NONE)
    {
    }

    DescriptorUsage(ShaderRegister slot, ShaderInputType type, Name setName, Name descriptorName, EnumFlags<DescriptorUsageFlags> flags = DescriptorUsageFlags::NONE, HashMap<String, String> params = {})
        : slot(slot),
          type(type),
          setName(setName),
          descriptorName(descriptorName),
          flags(flags),
          params(std::move(params))
    {
    }

    DescriptorUsage(const DescriptorUsage& other)
        : slot(other.slot),
          type(other.type),
          setName(other.setName),
          descriptorName(other.descriptorName),
          structureType(other.structureType),
          flags(other.flags),
          params(other.params)
    {
    }

    DescriptorUsage& operator=(const DescriptorUsage& other)
    {
        if (this == &other)
        {
            return *this;
        }

        slot = other.slot;
        type = other.type;
        setName = other.setName;
        descriptorName = other.descriptorName;
        structureType = other.structureType;
        flags = other.flags;
        params = other.params;

        return *this;
    }

    DescriptorUsage(DescriptorUsage&& other) noexcept
        : slot(other.slot),
          type(other.type),
          setName(std::move(other.setName)),
          descriptorName(std::move(other.descriptorName)),
          structureType(std::move(other.structureType)),
          flags(other.flags),
          params(std::move(other.params))
    {
    }

    DescriptorUsage& operator=(DescriptorUsage&& other) noexcept
    {
        if (this == &other)
        {
            return *this;
        }

        slot = other.slot;
        type = other.type;
        setName = std::move(other.setName);
        descriptorName = std::move(other.descriptorName);
        structureType = std::move(other.structureType);
        flags = other.flags;
        params = std::move(other.params);

        return *this;
    }

    ~DescriptorUsage() = default;

    HYP_FORCE_INLINE bool operator==(const DescriptorUsage& other) const
    {
        return slot == other.slot
            && type == other.type
            && setName == other.setName
            && descriptorName == other.descriptorName
            && structureType == other.structureType
            && flags == other.flags
            && params == other.params;
    }

    HYP_FORCE_INLINE bool operator!=(const DescriptorUsage& other) const
    {
        return slot != other.slot
            || type != other.type
            || setName != other.setName
            || descriptorName != other.descriptorName
            || structureType != other.structureType
            || flags != other.flags
            || params != other.params;
    }

    HYP_FORCE_INLINE bool operator<(const DescriptorUsage& other) const
    {
        if (slot != other.slot)
        {
            return slot < other.slot;
        }

        if (type != other.type)
        {
            return type < other.type;
        }

        if (setName != other.setName)
        {
            return setName < other.setName;
        }

        if (descriptorName != other.descriptorName)
        {
            return descriptorName < other.descriptorName;
        }

        if (structureType != other.structureType)
        {
            return structureType < other.structureType;
        }

        if (flags != other.flags)
        {
            return uint32(flags) < uint32(other.flags);
        }

        return false;
    }

    /*! \brief Returns true if this is a constant buffer or storage buffer. */
    HYP_FORCE_INLINE bool IsBuffer() const
    {
        return type == ShaderInputType::UNIFORM_BUFFER
            || type == ShaderInputType::UNIFORM_BUFFER_DYNAMIC
            || type == ShaderInputType::STORAGE_BUFFER
            || type == ShaderInputType::STORAGE_BUFFER_DYNAMIC;
    }

    HYP_FORCE_INLINE uint32 GetCount() const
    {
        uint32 value = 1;

        auto it = params.Find("count");

        if (it == params.End())
        {
            return value;
        }

        if (StringUtil::Parse(it->second, &value))
        {
            return value;
        }

        return 1;
    }

    HYP_FORCE_INLINE uint32 GetSize() const
    {
        if (structureType.HasExplicitSize())
        {
            return structureType.size;
        }

        uint32 value = ~0u;

        auto it = params.Find("size");

        if (it == params.End())
        {
            return value;
        }

        if (StringUtil::Parse(it->second, &value))
        {
            return value;
        }

        return uint32(-1);
    }

    HYP_FORCE_INLINE HashCode GetHashCode() const
    {
        HashCode hc;
        hc.Add(slot);
        hc.Add(type);
        hc.Add(setName.GetHashCode());
        hc.Add(descriptorName.GetHashCode());
        hc.Add(structureType);
        hc.Add(flags);
        hc.Add(params.GetHashCode());

        return hc;
    }
};

struct DescriptorUsageSet
{
    FlatSet<DescriptorUsage> elements;

    void BuildDescriptorTableDeclaration(ShaderInputGroup& table) const;

    HYP_FORCE_INLINE DescriptorUsage& operator[](SizeType index)
    {
        return elements[index];
    }

    HYP_FORCE_INLINE const DescriptorUsage& operator[](SizeType index) const
    {
        return elements[index];
    }

    HYP_FORCE_INLINE bool operator==(const DescriptorUsageSet& other) const
    {
        return elements == other.elements;
    }

    HYP_FORCE_INLINE bool operator!=(const DescriptorUsageSet& other) const
    {
        return elements != other.elements;
    }

    HYP_FORCE_INLINE SizeType Size() const
    {
        return elements.Size();
    }

    HYP_FORCE_INLINE void Add(const DescriptorUsage& descriptorUsage)
    {
        elements.Insert(descriptorUsage);
    }

    HYP_FORCE_INLINE DescriptorUsage* Find(StringHash descriptorName)
    {
        auto it = elements.FindIf([descriptorName](const DescriptorUsage& descriptorUsage)
            {
                return descriptorUsage.descriptorName == descriptorName;
            });

        if (it == elements.End())
        {
            return nullptr;
        }

        return it;
    }

    HYP_FORCE_INLINE const DescriptorUsage* Find(StringHash descriptorName) const
    {
        return const_cast<const DescriptorUsageSet*>(this)->Find(descriptorName);
    }

    HYP_FORCE_INLINE void Merge(const Array<DescriptorUsage>& other)
    {
        elements.Merge(other);
    }

    HYP_FORCE_INLINE void Merge(Array<DescriptorUsage>&& other)
    {
        elements.Merge(std::move(other));
    }

    HYP_FORCE_INLINE void Merge(const DescriptorUsageSet& other)
    {
        elements.Merge(other.elements);
    }

    HYP_FORCE_INLINE void Merge(DescriptorUsageSet&& other)
    {
        elements.Merge(std::move(other.elements));
    }

    HYP_FORCE_INLINE HashCode GetHashCode() const
    {
        return elements.GetHashCode();
    }
};

#pragma endregion Internal structures

#pragma region CompiledShader

CompiledShader::CompiledShader(const CompiledShader& other)
    : name(other.name),
      properties(other.properties),
      vertexAttributes(other.vertexAttributes),
      inputGroup(other.inputGroup),
      moduleTypes(other.moduleTypes),
      moduleNames(other.moduleNames),
      entryPointNames(other.entryPointNames),
      shaderBlobs(other.shaderBlobs),
      propertySetHashCode(other.propertySetHashCode)
{
}

CompiledShader& CompiledShader::operator=(const CompiledShader& other)
{
    if (this != &other)
    {
        name = other.name;
        properties = other.properties;
        vertexAttributes = other.vertexAttributes;
        inputGroup = other.inputGroup;
        moduleTypes = other.moduleTypes;
        moduleNames = other.moduleNames;
        entryPointNames = other.entryPointNames;
        shaderBlobs = other.shaderBlobs;
        propertySetHashCode = other.propertySetHashCode;
    }

    return *this;
}

CompiledShader::CompiledShader(CompiledShader&& other) noexcept
    : name(other.name),
      properties(other.properties),
      vertexAttributes(other.vertexAttributes),
      inputGroup(std::move(other.inputGroup)),
      moduleTypes(std::move(other.moduleTypes)),
      moduleNames(std::move(other.moduleNames)),
      entryPointNames(std::move(other.entryPointNames)),
      shaderBlobs(std::move(other.shaderBlobs)),
      propertySetHashCode(other.propertySetHashCode)
{
}

CompiledShader& CompiledShader::operator=(CompiledShader&& other) noexcept
{
    if (this != &other)
    {
        name = other.name;
        properties = other.properties;
        vertexAttributes = other.vertexAttributes;
        moduleTypes = std::move(other.moduleTypes);
        moduleNames = std::move(other.moduleNames);
        entryPointNames = std::move(other.entryPointNames);
        shaderBlobs = std::move(other.shaderBlobs);
        inputGroup = std::move(other.inputGroup);
        propertySetHashCode = other.propertySetHashCode;
    }
    return *this;
}

CompiledShader::~CompiledShader()
{
}

void CompiledShader::AddShaderModule(
    ShaderModuleType moduleType,
    UTF8StringView moduleName,
    UTF8StringView entryPointName,
    ByteBuffer&& shaderBlob)
{
    auto it = moduleTypes.Find(moduleType);

    // if we already have this module type, replace the blob and entry point name
    if (it != moduleTypes.End())
    {
        const SizeType index = it - moduleTypes.Begin();

        Assert(index < shaderBlobs.Size() && index < entryPointNames.Size());
        
        moduleNames[index] = moduleName;
        shaderBlobs[index] = std::move(shaderBlob);
        entryPointNames[index] = entryPointName;

        return;
    }

    Assert(moduleTypes.Size() == shaderBlobs.Size()
        && moduleTypes.Size() == moduleNames.Size()
        && moduleTypes.Size() == entryPointNames.Size());

    moduleTypes.PushBack(moduleType);
    moduleNames.PushBack(moduleName);
    entryPointNames.PushBack(entryPointName);
    shaderBlobs.PushBack(std::move(shaderBlob));
}

uint64 CompiledShader::GetRevisionNumber() const
{
    return GetStaticDescriptorTableDeclaration().GetHashCode().Value();
}

#pragma endregion CompiledShader

#pragma region DescriptorUsageSet

void DescriptorUsageSet::BuildDescriptorTableDeclaration(ShaderInputGroup& table) const
{
    for (const DescriptorUsage& descriptorUsage : elements)
    {
        Assert(descriptorUsage.slot != ShaderRegister::NONE && descriptorUsage.slot < ShaderRegister::MAX,
            "Descriptor usage {} has invalid slot {}",
            descriptorUsage.descriptorName.LookupString(), descriptorUsage.slot);

        DescriptorSetDeclaration* descriptorSetDeclaration = table.FindDescriptorSetDeclaration(descriptorUsage.setName);

        // check if this descriptor set is defined in the static descriptor table
        // if it is, we can use those definitions
        // otherwise, it is a 'custom' descriptor set
        DescriptorSetDeclaration* staticDescriptorSetDeclaration = GetStaticDescriptorTableDeclaration().FindDescriptorSetDeclaration(descriptorUsage.setName);

        if (staticDescriptorSetDeclaration != nullptr)
        {
            Assert(staticDescriptorSetDeclaration->FindDescriptorDeclaration(descriptorUsage.descriptorName) != nullptr,
                "Descriptor set {} is defined in the static descriptor table, but "
                "the descriptor {} is not",
                descriptorUsage.setName, descriptorUsage.descriptorName);

            if (!descriptorSetDeclaration)
            {
                const uint32 setIndex = uint32(table.elements.Size());

                DescriptorSetDeclaration newDescriptorSetDeclaration(setIndex, staticDescriptorSetDeclaration->name);
                newDescriptorSetDeclaration.flags = staticDescriptorSetDeclaration->flags | DescriptorSetDeclarationFlags::REFERENCE;

                table.AddDescriptorSetDeclaration(std::move(newDescriptorSetDeclaration));
            }

            continue;
        }

        if (!descriptorSetDeclaration)
        {
            const uint32 setIndex = uint32(table.elements.Size());

            descriptorSetDeclaration = table.AddDescriptorSetDeclaration(DescriptorSetDeclaration(setIndex, descriptorUsage.setName));
        }

        ShaderInput desc {};
        desc.slot = descriptorUsage.slot;
        desc.type = descriptorUsage.type;
        desc.name = descriptorUsage.descriptorName;
        desc.count = descriptorUsage.GetCount();
        desc.size = descriptorUsage.GetSize();
        desc.isDynamic = bool(descriptorUsage.flags & DescriptorUsageFlags::DYNAMIC);

        if (auto* existingDecl = descriptorSetDeclaration->FindDescriptorDeclaration(descriptorUsage.descriptorName))
        {
            // Already exists, just update the slot
            *existingDecl = std::move(desc);
        }
        else
        {
            descriptorSetDeclaration->AddDescriptorDeclaration(std::move(desc));
        }
    }
}

#pragma endregion DescriptorUsageSet

#pragma region SPRIV Compilation

#if HYP_VULKAN
static void GetSPIRVEnvironmentInfo(
    ShaderModuleType type,
#if HYP_GLSLANG
    uint32& outTargetApiVersion,
#endif
    uint32& outSpirvVersion,
    uint32& outVulkanVersion)
{
#if HYP_GLSLANG
    outTargetApiVersion = GLSLANG_TARGET_SPV_1_2;
#endif

    outSpirvVersion = 450;

    outVulkanVersion = HYP_VULKAN_API_VERSION;

    if (IsRayTracingShaderModule(type))
    {
#if HYP_GLSLANG
        outTargetApiVersion = MathUtil::Max(outTargetApiVersion, GLSLANG_TARGET_SPV_1_4);
#endif

        outSpirvVersion = MathUtil::Max(outSpirvVersion, 460);
        
        outVulkanVersion = MathUtil::Max(outVulkanVersion, VK_API_VERSION_1_2);
    }
}
#endif

#if HYP_GLSLANG && HYP_VULKAN

static void GetSPIRVEnvironmentInfo(ShaderModuleType type, uint32& outSpirvVersion, uint32& outVulkanVersion)
{
    uint32 dummy;
    GetSPIRVEnvironmentInfo(type, dummy, outSpirvVersion, outVulkanVersion);
}

static TBuiltInResource DefaultResources()
{
    return { /* .MaxLights = */ 32,
        /* .MaxClipPlanes = */ 6,
        /* .MaxTextureUnits = */ 32,
        /* .MaxTextureCoords = */ 32,
        /* .MaxVertexAttribs = */ 64,
        /* .MaxVertexUniformComponents = */ 4096,
        /* .MaxVaryingFloats = */ 64,
        /* .MaxVertexTextureImageUnits = */ 32,
        /* .MaxCombinedTextureImageUnits = */ 80,
        /* .MaxTextureImageUnits = */ 32,
        /* .MaxFragmentUniformComponents = */ 4096,
        /* .MaxDrawBuffers = */ 32,
        /* .MaxVertexUniformVectors = */ 128,
        /* .MaxVaryingVectors = */ 8,
        /* .MaxFragmentUniformVectors = */ 16,
        /* .MaxVertexOutputVectors = */ 16,
        /* .MaxFragmentInputVectors = */ 15,
        /* .MinProgramTexelOffset = */ -8,
        /* .MaxProgramTexelOffset = */ 7,
        /* .MaxClipDistances = */ 8,
        /* .MaxComputeWorkGroupCountX = */ 65535,
        /* .MaxComputeWorkGroupCountY = */ 65535,
        /* .MaxComputeWorkGroupCountZ = */ 65535,
        /* .MaxComputeWorkGroupSizeX = */ 1024,
        /* .MaxComputeWorkGroupSizeY = */ 1024,
        /* .MaxComputeWorkGroupSizeZ = */ 64,
        /* .MaxComputeUniformComponents = */ 1024,
        /* .MaxComputeTextureImageUnits = */ 16,
        /* .MaxComputeImageUniforms = */ 8,
        /* .MaxComputeAtomicCounters = */ 8,
        /* .MaxComputeAtomicCounterBuffers = */ 1,
        /* .MaxVaryingComponents = */ 60,
        /* .MaxVertexOutputComponents = */ 64,
        /* .MaxGeometryInputComponents = */ 64,
        /* .MaxGeometryOutputComponents = */ 128,
        /* .MaxFragmentInputComponents = */ 128,
        /* .MaxImageUnits = */ 8,
        /* .MaxCombinedImageUnitsAndFragmentOutputs = */ 8,
        /* .MaxCombinedShaderOutputResources = */ 8,
        /* .MaxImageSamples = */ 0,
        /* .MaxVertexImageUniforms = */ 0,
        /* .MaxTessControlImageUniforms = */ 0,
        /* .MaxTessEvaluationImageUniforms = */ 0,
        /* .MaxGeometryImageUniforms = */ 0,
        /* .MaxFragmentImageUniforms = */ 8,
        /* .MaxCombinedImageUniforms = */ 8,
        /* .MaxGeometryTextureImageUnits = */ 16,
        /* .MaxGeometryOutputVertices = */ 256,
        /* .MaxGeometryTotalOutputComponents = */ 1024,
        /* .MaxGeometryUniformComponents = */ 1024,
        /* .MaxGeometryVaryingComponents = */ 64,
        /* .MaxTessControlInputComponents = */ 128,
        /* .MaxTessControlOutputComponents = */ 128,
        /* .MaxTessControlTextureImageUnits = */ 16,
        /* .MaxTessControlUniformComponents = */ 1024,
        /* .MaxTessControlTotalOutputComponents = */ 4096,
        /* .MaxTessEvaluationInputComponents = */ 128,
        /* .MaxTessEvaluationOutputComponents = */ 128,
        /* .MaxTessEvaluationTextureImageUnits = */ 16,
        /* .MaxTessEvaluationUniformComponents = */ 1024,
        /* .MaxTessPatchComponents = */ 120,
        /* .MaxPatchVertices = */ 32,
        /* .MaxTessGenLevel = */ 64,
        /* .MaxViewports = */ 16,
        /* .MaxVertexAtomicCounters = */ 0,
        /* .MaxTessControlAtomicCounters = */ 0,
        /* .MaxTessEvaluationAtomicCounters = */ 0,
        /* .MaxGeometryAtomicCounters = */ 0,
        /* .MaxFragmentAtomicCounters = */ 8,
        /* .MaxCombinedAtomicCounters = */ 8,
        /* .MaxAtomicCounterBindings = */ 1,
        /* .MaxVertexAtomicCounterBuffers = */ 0,
        /* .MaxTessControlAtomicCounterBuffers = */ 0,
        /* .MaxTessEvaluationAtomicCounterBuffers = */ 0,
        /* .MaxGeometryAtomicCounterBuffers = */ 0,
        /* .MaxFragmentAtomicCounterBuffers = */ 1,
        /* .MaxCombinedAtomicCounterBuffers = */ 1,
        /* .MaxAtomicCounterBufferSize = */ 16384,
        /* .MaxTransformFeedbackBuffers = */ 4,
        /* .MaxTransformFeedbackInterleavedComponents = */ 64,
        /* .MaxCullDistances = */ 8,
        /* .MaxCombinedClipAndCullDistances = */ 8,
        /* .MaxSamples = */ 4,
        /* .maxMeshOutputVerticesNV = */ 256,
        /* .maxMeshOutputPrimitivesNV = */ 512,
        /* .maxMeshWorkGroupSizeX_NV = */ 32,
        /* .maxMeshWorkGroupSizeY_NV = */ 1,
        /* .maxMeshWorkGroupSizeZ_NV = */ 1,
        /* .maxTaskWorkGroupSizeX_NV = */ 32,
        /* .maxTaskWorkGroupSizeY_NV = */ 1,
        /* .maxTaskWorkGroupSizeZ_NV = */ 1,
        /* .maxMeshViewCountNV = */ 4,
        /* .maxMeshOutputVerticesEXT = */ 256,
        /* .maxMeshOutputPrimitivesEXT = */ 256,
        /* .maxMeshWorkGroupSizeX_EXT = */ 128,
        /* .maxMeshWorkGroupSizeY_EXT = */ 128,
        /* .maxMeshWorkGroupSizeZ_EXT = */ 128,
        /* .maxTaskWorkGroupSizeX_EXT = */ 128,
        /* .maxTaskWorkGroupSizeY_EXT = */ 128,
        /* .maxTaskWorkGroupSizeZ_EXT = */ 128,
        /* .maxMeshViewCountEXT = */ 4,
        /* .maxDualSourceDrawBuffersEXT = */ 1,

        /* .limits = */
        {
            /* .nonInductiveForLoops = */ 1,
            /* .whileLoops = */ 1,
            /* .doWhileLoops = */ 1,
            /* .generalUniformIndexing = */ 1,
            /* .generalAttributeMatrixVectorIndexing = */ 1,
            /* .generalVaryingIndexing = */ 1,
            /* .generalSamplerIndexing = */ 1,
            /* .generalVariableIndexing = */ 1,
            /* .generalConstantMatrixVectorIndexing = */ 1,
        } };
}

static bool PreprocessGLSL(
    ShaderModuleType type,
    const String& preamble,
    const String& source,
    const String& filename,
    String& outPreprocessedSource,
    Array<String>& outErrorMessages)
{

#define GLSL_ERROR(level, errorMessage, ...)                                \
    {                                                                       \
        HYP_LOG(ShaderCompiler, level, errorMessage, ##__VA_ARGS__);        \
        outErrorMessages.PushBack(HYP_FORMAT(errorMessage, ##__VA_ARGS__)); \
    }

    auto defaultResources = DefaultResources();

    glslang_stage_t stage;

    switch (type)
    {
    case ShaderModuleType::Vertex:
        stage = GLSLANG_STAGE_VERTEX;
        break;
    case ShaderModuleType::Pixel:
        stage = GLSLANG_STAGE_FRAGMENT;
        break;
    case ShaderModuleType::Geometry:
        stage = GLSLANG_STAGE_GEOMETRY;
        break;
    case ShaderModuleType::Compute:
        stage = GLSLANG_STAGE_COMPUTE;
        break;
    case ShaderModuleType::Task:
        stage = GLSLANG_STAGE_TASK_NV;
        break;
    case ShaderModuleType::Mesh:
        stage = GLSLANG_STAGE_MESH_NV;
        break;
    case ShaderModuleType::TessControl:
        stage = GLSLANG_STAGE_TESSCONTROL;
        break;
    case ShaderModuleType::TessEval:
        stage = GLSLANG_STAGE_TESSEVALUATION;
        break;
    case ShaderModuleType::RayGen:
        stage = GLSLANG_STAGE_RAYGEN_NV;
        break;
    case ShaderModuleType::Intersect:
        stage = GLSLANG_STAGE_INTERSECT_NV;
        break;
    case ShaderModuleType::AnyHit:
        stage = GLSLANG_STAGE_ANYHIT_NV;
        break;
    case ShaderModuleType::ClosestHit:
        stage = GLSLANG_STAGE_CLOSESTHIT_NV;
        break;
    case ShaderModuleType::Miss:
        stage = GLSLANG_STAGE_MISS_NV;
        break;
    default:
        HYP_THROW("Invalid shader type");
        break;
    }
    
    uint32 spirvApiVersion;
    uint32 spirvVersion;
    uint32 vulkanApiVersion;
    GetSPIRVEnvironmentInfo(type, spirvApiVersion, spirvVersion, vulkanApiVersion);

    struct CallbacksContext
    {
        String filename;

        Stack<Proc<void()>> deleters;

        ~CallbacksContext()
        {
            // Run all deleters to free memory allocated in callbacks
            while (!deleters.Empty())
            {
                deleters.Pop()();
            }
        }
    } callbacksContext;

    callbacksContext.filename = filename;

    glslang_input_t input {
        .language = GLSLANG_SOURCE_GLSL,
        .stage = stage,
        .client = GLSLANG_CLIENT_VULKAN,
        .client_version = static_cast<glslang_target_client_version_t>(vulkanApiVersion),
        .target_language = GLSLANG_TARGET_SPV,
        .target_language_version = static_cast<glslang_target_language_version_t>(spirvApiVersion),
        .code = source.Data(),
        .default_version = int(spirvVersion),
        .default_profile = GLSLANG_CORE_PROFILE,
        .force_default_version_and_profile = false,
        .forward_compatible = false,
        .messages = GLSLANG_MSG_DEFAULT_BIT,
        .resource = reinterpret_cast<const glslang_resource_t*>(&defaultResources),
        .callbacks_ctx = &callbacksContext
    };

    input.callbacks.include_local =
        [](void* ctx, const char* headerName, const char* includerName, size_t includeDepth) -> glsl_include_result_t*
    {
        CallbacksContext* callbacksContext = static_cast<CallbacksContext*>(ctx);

        const FilePath basePath = FilePath(callbacksContext->filename).BasePath();

        const FilePath dir = includeDepth > 1
            ? FilePath(includerName).BasePath()
            : GetResourceDirectory() / FilePath::Relative(basePath, GetResourceDirectory());

        const FilePath path = dir / headerName;

        if (!path.Exists())
        {
            HYP_LOG(ShaderCompiler, Warning,
                "File at path {} does not exist, cannot include file {}", path,
                headerName);

            return nullptr;
        }

        FileBufferedReaderSource source { path };
        BufferedReader reader { &source };

        if (!reader.IsOpen())
        {
            HYP_LOG(ShaderCompiler, Warning, "Failed to open include file {}", path);

            return nullptr;
        }

        String linesJoined = String::Join(reader.ReadAllLines(), '\n');

        glsl_include_result_t* result = new glsl_include_result_t;

        char* headerNameStr = new char[path.Size() + 1];
        Memory::Fill(headerNameStr, 0, path.Size() + 1);
        Memory::StrCpy(headerNameStr, path.Data(), path.Size());
        result->header_name = headerNameStr;

        char* headerDataStr = new char[linesJoined.Size() + 1];
        Memory::Fill(headerDataStr, 0, linesJoined.Size() + 1);
        Memory::StrCpy(headerDataStr, linesJoined.Data(), linesJoined.Size());
        result->header_data = headerDataStr;

        result->header_length = linesJoined.Size();

        callbacksContext->deleters.Push([result]
            {
                delete[] result->header_name;
                delete[] result->header_data;
                delete result;
            });

        return result;
    };

    glslang_shader_t* shader = glslang_shader_create(&input);

    glslang_shader_set_preamble(shader, preamble.Data());

    if (!glslang_shader_preprocess(shader, &input))
    {
        GLSL_ERROR(Error, "GLSL preprocessing failed {}", filename);
        GLSL_ERROR(Error, "{}", glslang_shader_get_info_log(shader));
        GLSL_ERROR(Error, "{}", glslang_shader_get_info_debug_log(shader));

        glslang_shader_delete(shader);

        return false;
    }

    outPreprocessedSource = glslang_shader_get_preprocessed_code(shader);

    // HYP_LOG(ShaderCompiler, Debug, "Preprocessed source for {}: Before:
    // \n{}\nAfter:\n{}", filename, source, outPreprocessedSource);

    glslang_shader_delete(shader);

#undef GLSL_ERROR

    return true;
}

static ByteBuffer CompileGLSL(
    ShaderModuleType type,
    DescriptorUsageSet& descriptorUsages,
    String source, String filename,
    Array<String>& errorMessages)
{
#define GLSL_ERROR(level, errorMessage, ...)                             \
    {                                                                    \
        HYP_LOG(ShaderCompiler, level, errorMessage, ##__VA_ARGS__);     \
        errorMessages.PushBack(HYP_FORMAT(errorMessage, ##__VA_ARGS__)); \
    }

    auto defaultResources = DefaultResources();

    glslang_stage_t stage;
    String stageString;

    switch (type)
    {
    case ShaderModuleType::Vertex:
        stage = GLSLANG_STAGE_VERTEX;
        stageString = "VERTEX_SHADER";
        break;
    case ShaderModuleType::Pixel:
        stage = GLSLANG_STAGE_FRAGMENT;
        stageString = "PIXEL_SHADER";
        break;
    case ShaderModuleType::Geometry:
        stage = GLSLANG_STAGE_GEOMETRY;
        stageString = "GEOMETRY_SHADER";
        break;
    case ShaderModuleType::Compute:
        stage = GLSLANG_STAGE_COMPUTE;
        stageString = "COMPUTE_SHADER";
        break;
    case ShaderModuleType::Task:
        stage = GLSLANG_STAGE_TASK_NV;
        stageString = "TASK_SHADER";
        break;
    case ShaderModuleType::Mesh:
        stage = GLSLANG_STAGE_MESH_NV;
        stageString = "MESH_SHADER";
        break;
    case ShaderModuleType::TessControl:
        stage = GLSLANG_STAGE_TESSCONTROL;
        stageString = "TESS_CONTROL_SHADER";
        break;
    case ShaderModuleType::TessEval:
        stage = GLSLANG_STAGE_TESSEVALUATION;
        stageString = "TESS_EVAL_SHADER";
        break;
    case ShaderModuleType::RayGen:
        stage = GLSLANG_STAGE_RAYGEN_NV;
        stageString = "RAY_GEN_SHADER";
        break;
    case ShaderModuleType::Intersect:
        stage = GLSLANG_STAGE_INTERSECT_NV;
        stageString = "RAY_INTERSECT_SHADER";
        break;
    case ShaderModuleType::AnyHit:
        stage = GLSLANG_STAGE_ANYHIT_NV;
        stageString = "RAY_ANY_HIT_SHADER";
        break;
    case ShaderModuleType::ClosestHit:
        stage = GLSLANG_STAGE_CLOSESTHIT_NV;
        stageString = "RAY_CLOSEST_HIT_SHADER";
        break;
    case ShaderModuleType::Miss:
        stage = GLSLANG_STAGE_MISS_NV;
        stageString = "RAY_MISS_SHADER";
        break;
    default:
        HYP_THROW("Invalid shader type");
        break;
    }

    // fallback to allow compiling shaders for vulkan targets when not compiled with vulkan support.
#if !HYP_VULKAN
#ifndef VK_API_VERSION_1_1
    static constexpr uint32 VK_API_VERSION_1_1 = 4198400;
#endif
#ifndef VK_API_VERSION_1_1
    static constexpr uint32 VK_API_VERSION_1_2 = 4202496;
#endif
    
    static constexpr uint32 HYP_VULKAN_API_VERSION = VK_API_VERSION_1_2;
#endif

    uint32 vulkanApiVersion = HYP_VULKAN_API_VERSION;

    uint32 spirvApiVersion = GLSLANG_TARGET_SPV_1_2;
    uint32 spirvVersion = 450;

    if (IsRayTracingShaderModule(type))
    {
        vulkanApiVersion = MathUtil::Max(vulkanApiVersion, VK_API_VERSION_1_2);

        spirvApiVersion = MathUtil::Max(spirvApiVersion, GLSLANG_TARGET_SPV_1_4);
        spirvVersion = MathUtil::Max(spirvVersion, 460);
    }

    glslang_input_t input {
        .language = GLSLANG_SOURCE_GLSL,
        .stage = stage,
        .client = GLSLANG_CLIENT_VULKAN,
        .client_version = static_cast<glslang_target_client_version_t>(vulkanApiVersion),
        .target_language = GLSLANG_TARGET_SPV,
        .target_language_version = static_cast<glslang_target_language_version_t>(spirvApiVersion),
        .code = source.Data(),
        .default_version = int(spirvVersion),
        .default_profile = GLSLANG_CORE_PROFILE,
        .force_default_version_and_profile = false,
        .forward_compatible = false,
        .messages = GLSLANG_MSG_DEFAULT_BIT,
        .resource = reinterpret_cast<const glslang_resource_t*>(&defaultResources),
        .callbacks_ctx = nullptr
    };

    glslang_shader_t* shader = glslang_shader_create(&input);

    ShaderInputGroup table;
    descriptorUsages.BuildDescriptorTableDeclaration(table);

    String preamble = BuildDescriptorTableDefines(ShaderLanguage::GLSL, table);

    glslang_shader_set_preamble(shader, preamble.Data());

    if (!glslang_shader_preprocess(shader, &input))
    {
        GLSL_ERROR(Error, "GLSL preprocessing failed {}", filename);
        GLSL_ERROR(Error, "{}", glslang_shader_get_info_log(shader));
        GLSL_ERROR(Error, "{}", glslang_shader_get_info_debug_log(shader));

        glslang_shader_delete(shader);

        return ByteBuffer();
    }
    
    String preprocessed = glslang_shader_get_preprocessed_code(shader);

    if (!glslang_shader_parse(shader, &input))
    {
        GLSL_ERROR(Error, "GLSL parsing failed {}", filename);
        GLSL_ERROR(Error, "{}", glslang_shader_get_info_log(shader));
        GLSL_ERROR(Error, "{}", glslang_shader_get_info_debug_log(shader));

        glslang_shader_delete(shader);

        return ByteBuffer();
    }

    glslang_program_t* program = glslang_program_create();
    glslang_program_add_shader(program, shader);
    
    if (!glslang_program_link(program, GLSLANG_MSG_SPV_RULES_BIT | GLSLANG_MSG_VULKAN_RULES_BIT))
    {
        GLSL_ERROR(Error, "GLSL linking failed {} {}", filename, source);
        GLSL_ERROR(Error, "{}", glslang_program_get_info_log(program));
        GLSL_ERROR(Error, "{}", glslang_program_get_info_debug_log(program));

        glslang_program_delete(program);
        glslang_shader_delete(shader);

        return ByteBuffer();
    }

    const char* entryPointName = DefaultEntryPointNames[uint8(type)];

    glslang::TProgram* cppProgram = glslang_get_cpp_program(program);
    Assert(cppProgram != nullptr);

#if HYP_SHADER_REFLECTION
    if (!cppProgram->buildReflection(EShReflectionDefault))
    {
        GLSL_ERROR(Error, "Failed to build shader reflection!");
    }
#endif
    
    glslang_spv_options_t spvOptions {};
    spvOptions.disable_optimizer = true;
#ifdef HYP_DEBUG_MODE
    spvOptions.generate_debug_info = true;
    spvOptions.strip_debug_info = false;
    spvOptions.validate = true;
#endif

    glslang_program_SPIRV_generate_with_options(program, stage, &spvOptions);

#ifdef HYP_SHADER_REFLECTION
    Proc<void(const glslang::TType*, StructureType&)> HandleShaderStruct;

    HandleShaderStruct = [&HandleShaderStruct](const glslang::TType* type, StructureType& outStructureType)
    {
        if (type->isStruct())
        {
            for (auto it = type->getStruct()->begin(); it != type->getStruct()->end(); ++it)
            {
                String fieldTypeName;

                if (it->type->isStruct())
                {
                    fieldTypeName = it->type->getTypeName().data();
                }
                else
                {
                    fieldTypeName = it->type->getCompleteString(true, false, false, true).data();
                }

                auto& field = outStructureType.AddField(
                                                    CreateNameFromDynamicString(it->type->getFieldName().data()),
                                                    StructureType(CreateNameFromDynamicString(fieldTypeName)))
                                    .second;

                HandleShaderStruct(it->type, field);
            }
        }
    };

    // inject reflection info for shader inputs
    for (DescriptorUsage& usage : descriptorUsages.elements)
    {
        const char* duNameString = usage.descriptorName.LookupString();
        const int reflectionIndex = cppProgram->getReflectionIndex(duNameString);

        if (reflectionIndex == -1)
        {
            continue;
        }

        const glslang::TObjectReflection* refl = nullptr;

        if (usage.IsBuffer())
        {
            refl = &cppProgram->getUniformBlock(reflectionIndex);

            if (!refl)
            {
                refl = &cppProgram->getBufferBlock(reflectionIndex);
            }

            if (!refl->getType())
            {
                continue;
            }

            AssertDebug(refl->getType()->getTypeName() == duNameString);
        }

        if (refl != nullptr)
        {
            HandleShaderStruct(refl->getType(), usage.structureType);
            usage.structureType.size = refl->size;

            continue;
        }
    }
#endif

    ByteBuffer shaderModule(glslang_program_SPIRV_get_size(program) * sizeof(uint32));
    glslang_program_SPIRV_get(program, reinterpret_cast<uint32*>(shaderModule.Data()));

    const char* spirvMessages = glslang_program_SPIRV_get_messages(program);

    if (spirvMessages)
    {
        GLSL_ERROR(Error, "{}:\n{}", filename, spirvMessages);
    }

    glslang_program_delete(program);
    glslang_shader_delete(shader);

#undef GLSL_ERROR

    if (filename.EndsWith(".hlsl"))
    {
        Assert(shaderModule.Size() > 0);
        HYP_LOG(ShaderCompiler, Info, "Processed source for {}:\n\n{}\n\n",
            filename, preprocessed);
    }


    return shaderModule;
}

#endif // HYP_GLSLANG

#if HYP_DXC

#if HYP_SHADER_REFLECTION

static void ReflectResources(ID3D12ShaderReflection* pReflection, DescriptorUsageSet& outUsages)
{
    D3D12_SHADER_DESC shaderDesc;
    pReflection->GetDesc(&shaderDesc);

    for (UINT i = 0; i < shaderDesc.BoundResources; i++)
    {
        D3D12_SHADER_INPUT_BIND_DESC resDesc;
        pReflection->GetResourceBindingDesc(i, &resDesc);

        DescriptorUsage* descriptorUsage = outUsages.Find(CreateStringHashFromDynamicString(resDesc.Name));

        if (descriptorUsage != nullptr)
        {
            if (resDesc.Type == D3D_SIT_CBUFFER)
            {
                ID3D12ShaderReflectionConstantBuffer* pCB = pReflection->GetConstantBufferByName(resDesc.Name);
                D3D12_SHADER_BUFFER_DESC bufferDesc;
                pCB->GetDesc(&bufferDesc);
                
                descriptorUsage->structureType.size = (SizeType)bufferDesc.Size;
                
                for (UINT j = 0; j < bufferDesc.Variables; j++)
                {
                    ID3D12ShaderReflectionVariable* pVar = pCB->GetVariableByIndex(j);
                    D3D12_SHADER_VARIABLE_DESC varDesc;
                    pVar->GetDesc(&varDesc);
                    
                    // TODO!!!
                }
            }
        }
    }
}
#endif // HYP_SHADER_REFLECTION

static bool PreprocessHLSL(
    ShaderModuleType type,
    const String& preamble,
    const String& source,
    const String& filename,
    String& outPreprocessedSource,
    Array<String>& outErrorMessages)
{
    Assert(s_dxcCompiler && s_dxcUtils);

    outPreprocessedSource = source;

    WideString includeDirs[] = {
        WideString(FilePath(filename).BasePath()),
        WideString(GetResourceDirectory() / "shaders"),
        WideString(GetResourceDirectory() / "shaders" / "include")
    };

    Array<LPCWSTR> args;

    for (const WideString& str : includeDirs)
    {
        args.PushBack(L"-I");
        args.PushBack(*str);
    }

    args.PushBack(L"-P");

    String fullSource = preamble + "\n" + source;

    HRESULT hr;

    ComPtr<IDxcBlobEncoding> pSource;
    hr = s_dxcUtils->CreateBlobFromPinned(fullSource.Data(), (uint32)fullSource.Size(), CP_UTF8, &pSource);

    if (FAILED(hr))
    {
        outErrorMessages.PushBack(HYP_FORMAT("Failed to create blob! HRESULT: {}", hr));

        return false;
    }

    DxcBuffer sourceBuffer = { pSource->GetBufferPointer(), pSource->GetBufferSize(), 0 };

    ComPtr<IDxcResult> pResult;

    ComPtr<IDxcIncludeHandler> pIncludeHandler;
    hr = s_dxcUtils->CreateDefaultIncludeHandler(&pIncludeHandler);

    if (FAILED(hr))
    {
        outErrorMessages.PushBack(HYP_FORMAT("Failed to create default include handler! HRESULT: {}", hr));

        return false;
    }

    hr = s_dxcCompiler->Compile(
        &sourceBuffer,
        args.Data(),
        (uint32)args.Size(),
        pIncludeHandler.Get(),
        IID_PPV_ARGS(&pResult)
    );

    ComPtr<IDxcBlobUtf8> pErrors;
    pResult->GetOutput(DXC_OUT_ERRORS, IID_PPV_ARGS(&pErrors), nullptr);

    if (pErrors && pErrors->GetStringLength() > 0)
        outErrorMessages.PushBack(String(pErrors->GetStringPointer()));

    HRESULT status;
    pResult->GetStatus(&status);

    if (FAILED(status))
        return false;

    ComPtr<IDxcBlobUtf8> pOutput;
    pResult->GetOutput(DXC_OUT_HLSL, IID_PPV_ARGS(&pOutput), nullptr);

    if (pOutput)
    {
        outPreprocessedSource = String(pOutput->GetStringPointer());
        return true;
    }

    return false;
}

enum class HLSLOutputType
{
    SPIRV,
    DXIL
};

HYP_DISABLE_OPTIMIZATION;
static ByteBuffer CompileHLSL(
    ShaderModuleType type,
    HLSLOutputType outputType,
    DescriptorUsageSet& descriptorUsages,
    String source, String filename,
    const ShaderVariantPerms& perm,
    Array<String>& errorMessages)
{
    Assert(s_dxcCompiler && s_dxcUtils);

    ShaderInputGroup table;
    descriptorUsages.BuildDescriptorTableDeclaration(table);

    String preamble = BuildDescriptorTableDefines(ShaderLanguage::HLSL, table)
        + "\n" + BuildAttributesDefines(ShaderLanguage::HLSL, perm);

    String fullSource = preamble + "\n" + source;

    ComPtr<IDxcBlobEncoding> pSource;
    s_dxcUtils->CreateBlobFromPinned(fullSource.Data(), (uint32)fullSource.Size(), CP_UTF8, &pSource);

    const WideString entryPointName = WideString(DefaultEntryPointNames[uint8(type)]);

    Array<LPCWSTR> args;

    args.PushBack(L"-E");
    args.PushBack(*entryPointName);

    args.PushBack(L"-T");
    args.PushBack(GetDXCTargetProfile(type));

    args.PushBack(L"-HV 2021");
    
#if HYP_VULKAN
    if (outputType == HLSLOutputType::SPIRV)
    {
        args.PushBack(L"-spirv");

        uint32 spirvVersion;
        uint32 vulkanApiVersion;
        GetSPIRVEnvironmentInfo(type, spirvVersion, vulkanApiVersion);

        switch (vulkanApiVersion)
        {
        case VK_API_VERSION_1_0:
            args.PushBack(L"-fspv-target-env=vulkan1.0");
            break;
        case VK_API_VERSION_1_1:
            args.PushBack(L"-fspv-target-env=vulkan1.1");
            break;
        case VK_API_VERSION_1_2:
            args.PushBack(L"-fspv-target-env=vulkan1.2");
            break;
        case VK_API_VERSION_1_3:
            args.PushBack(L"-fspv-target-env=vulkan1.3");
            break;
        case VK_API_VERSION_1_4:
            args.PushBack(L"-fspv-target-env=vulkan1.4");
            break;
        default:
            errorMessages.PushBack(HYP_FORMAT("Unsupported vulkan version {}", vulkanApiVersion));
            return {};
        }

        args.PushBack(L"-fvk-use-scalar-layout");
    }
#endif

#ifdef HYP_DEBUG_MODE
    args.PushBack(L"-Zsb");
#endif

    DxcBuffer sourceBuffer = { pSource->GetBufferPointer(), pSource->GetBufferSize(), 0 };
    ComPtr<IDxcResult> pResult;
    
    HRESULT res = s_dxcCompiler->Compile(
        &sourceBuffer, 
        args.Data(), 
        (uint32)args.Size(), 
        nullptr,
        IID_PPV_ARGS(&pResult)
    );

    ComPtr<IDxcBlobUtf8> pErrors;
    pResult->GetOutput(DXC_OUT_ERRORS, IID_PPV_ARGS(&pErrors), nullptr);

    if (pErrors && pErrors->GetStringLength() != 0)
    {
        errorMessages.PushBack(String(pErrors->GetStringPointer()));
    }

    pResult->GetStatus(&res);

    if (FAILED(res))
    {
        errorMessages.PushBack(HYP_FORMAT("Failed to compile HLSL shader {}, HRESULT: {}", filename, res));

        return ByteBuffer();
    }

    ComPtr<IDxcBlob> pReflectionData;
    res = pResult->GetOutput(DXC_OUT_REFLECTION, IID_PPV_ARGS(&pReflectionData), nullptr);
    
    if (SUCCEEDED(res) && pReflectionData)
    {
        DxcBuffer reflBuffer = { pReflectionData->GetBufferPointer(), pReflectionData->GetBufferSize(), 0 };
        ComPtr<ID3D12ShaderReflection> pReflection;
        s_dxcUtils->CreateReflection(&reflBuffer, IID_PPV_ARGS(&pReflection));

        ReflectResources(pReflection.Get(), descriptorUsages);
    }

    ComPtr<IDxcBlob> pBlob;
    pResult->GetOutput(DXC_OUT_OBJECT, IID_PPV_ARGS(&pBlob), nullptr);

    ByteBuffer bytecode(pBlob->GetBufferSize());
    Assert(bytecode.Size() > 0);

    Memory::Copy(bytecode.Data(), pBlob->GetBufferPointer(), pBlob->GetBufferSize());

    return bytecode;
}

HYP_ENABLE_OPTIMIZATION;
#endif // HYP_DXC

#pragma endregion SPRIV Compilation

struct LoadedSourceFile
{
    ShaderModuleType type;
    ShaderLanguage language;
    String file;
    Time lastModifiedTimestamp;
    String source;

    FilePath GetOutputFilepath(const CompiledShader& compiledShader) const
    {
        HashCode hc;
        hc.Add(file);
        hc.Add(compiledShader.properties.GetHashCode());

        return GetTempDirectory() / FilePath(file).Basename() + "_" + (String::ToString(hc.Value()) + ".bin");
    }

    HashCode GetHashCode() const
    {
        HashCode hc;
        hc.Add(type);
        hc.Add(language);
        hc.Add(file);
        hc.Add(lastModifiedTimestamp);
        hc.Add(source);

        return hc;
    }
};

static const FlatMap<String, ShaderModuleType> s_shaderTypeNames = {
    { "vs", ShaderModuleType::Vertex },
    { "ps", ShaderModuleType::Pixel },
    { "gs", ShaderModuleType::Geometry },
    { "cs", ShaderModuleType::Compute },
    { "raygen", ShaderModuleType::RayGen },
    { "closesthit", ShaderModuleType::ClosestHit },
    { "anyhit", ShaderModuleType::AnyHit },
    { "miss", ShaderModuleType::Miss },
    { "intersect", ShaderModuleType::Intersect }
};

static bool FindVertexAttributeForDefinition(const String& name, const VertexAttribute*& outAttribute)
{
    for (const VertexAttribute** attr = VertexAttribute::Attrs; *attr; attr++)
    {
        if (name == (*attr)->name)
        {
            outAttribute = *attr;
            return true;
        }
    }

    return false;
}

static VertexAttributeSet BuildVertexAttributeSet(
    const Array<VertexAttributeDefinition>& definitions)
{
    VertexAttributeSet set;

    for (const VertexAttributeDefinition& definition : definitions)
    {
        const VertexAttribute* attr = nullptr;

        if (!FindVertexAttributeForDefinition(definition.name, attr))
        {
            HYP_LOG(ShaderCompiler, Error, "Invalid vertex attribute definition, {}", definition.name);

            continue;
        }

        Assert(attr != nullptr);

        set |= *attr;
    }

    return set;
}

static void ForEachPermutation(
    const ShaderVariantPerms& versions,
    const ProcRef<void(const ShaderVariantPerms&)>& callback,
    bool parallel)
{
    Array<ShaderProperty> variableProperties;
    Array<ShaderProperty> staticProperties;
    Array<ShaderProperty> valueGroups;

    for (const VertexAttribute** ppAttr = VertexAttribute::Attrs; *ppAttr; ppAttr++)
    {
        const VertexAttribute& attr = **ppAttr;

        if (versions.HasRequiredVertexAttribute(attr))
        {
            staticProperties.PushBack(ShaderProperty(attr));
        }
        else if (versions.HasOptionalVertexAttribute(attr))
        {
            variableProperties.PushBack(ShaderProperty(attr));
        }
    }

    for (const ShaderProperty& property : versions.GetPropertySet())
    {
        if (property.IsValueGroup())
        {
            valueGroups.PushBack(property);
        }
        else if (property.IsPermutable())
        {
            variableProperties.PushBack(property);
        }
        else
        {
            staticProperties.PushBack(property);
        }
    }

    const SizeType numPermutations = 1ull << variableProperties.Size();

    Array<ShaderVariantPerms> propertiesBeforeValueGroups;

    {
        SizeType initialCount = numPermutations;

        for (const ShaderProperty& valueGroup : valueGroups)
        {
            initialCount += valueGroup.enumValues.Size() * initialCount;
        }

        propertiesBeforeValueGroups.Reserve(initialCount);
    }

    Array<ShaderVariantPerms>* currentCombinations = &propertiesBeforeValueGroups;

    for (SizeType i = 0; i < numPermutations; i++)
    {
        HashSet<ShaderProperty> currentProperties;
        currentProperties.Reserve(ByteUtil::BitCount(i) + staticProperties.Size());
        currentProperties.Merge(staticProperties);

        for (SizeType j = 0; j < variableProperties.Size(); j++)
        {
            if (i & (1ull << j))
            {
                AssertDebug(!variableProperties[j].IsValueGroup());

                currentProperties.Insert(variableProperties[j]);
            }
        }

        currentCombinations->EmplaceBack(std::move(currentProperties));
    }

    Array<ShaderVariantPerms> propertiesWithValueGroupsApplied = propertiesBeforeValueGroups;
    currentCombinations = &propertiesWithValueGroupsApplied;

    // now apply the value groups onto it
    for (const ShaderProperty& valueGroup : valueGroups)
    {
        Array<ShaderVariantPerms> currentGroupPerms;
        currentGroupPerms.Resize(valueGroup.enumValues.Size() * currentCombinations->Size());

        for (SizeType existingCombinationIndex = 0; existingCombinationIndex < currentCombinations->Size(); existingCombinationIndex++)
        {
            for (SizeType valueIndex = 0; valueIndex < valueGroup.enumValues.Size(); valueIndex++)
            {
                // copy the current version of the array
                ShaderVariantPerms merged = (*currentCombinations)[existingCombinationIndex];

                AssertDebug(!merged.Has(valueGroup.name), "Duplicate shader property name detected for {}! This will cause shader compilation errors", valueGroup.name);

                const ShaderProperty::Value& shaderVal = valueGroup.enumValues[valueIndex];

                merged.Set(ShaderProperty(valueGroup.name, shaderVal));

                currentGroupPerms[existingCombinationIndex + (valueIndex * currentCombinations->Size())] = std::move(merged);
            }
        }

#ifdef HYP_SHADER_COMPILER_LOGGING
        HYP_LOG(ShaderCompiler, Info,
            "\tShader value group {} has {} permutations:", valueGroup.name,
            currentGroupPerms.Size());

        for (const ShaderVariantPerms& perm : currentGroupPerms)
        {
            HYP_LOG(ShaderCompiler, Debug, "\t\t{}", perm.ToString());
        }
#endif

        *currentCombinations = std::move(currentGroupPerms);
    }

#ifdef HYP_SHADER_COMPILER_LOGGING
    HYP_LOG(ShaderCompiler, Info,
        "Processing {} shader permutations:", currentCombinations->Size());
#endif

    if (parallel)
    {
        TaskSystem::GetInstance().ParallelForEach(
            *currentCombinations, [&callback](const ShaderVariantPerms& perm, uint32, uint32)
            {
                callback(perm);
            });
    }
    else
    {
        for (const ShaderVariantPerms& perm : *currentCombinations)
        {
            callback(perm);
        }
    }
}

static bool LoadBundleFromFilePath(const FilePath& filePath, ShaderBundle& outBundle)
{
    // read file if it already exists.
    FBOMReader reader { FBOMReaderConfig {} };

    FBOMResult err;

    BoxedValue value;

    if ((err = reader.LoadFromFile(filePath, value)))
    {
        HYP_LOG(ShaderCompiler, Error, "Failed to load shader at path: {}\n\tMessage: {}", filePath, err.message);

        return false;
    }

    Optional<ShaderBundle&> bundleOpt = value.TryGet<ShaderBundle>();

    if (!bundleOpt.HasValue())
    {
        HYP_LOG(ShaderCompiler, Error,
            "Failed to load compiled shader at path: {}\n\tMessage: {}",
            filePath, "Failed to deserialize ShaderBundle");

        return false;
    }

    outBundle = *bundleOpt;

    return true;
}

#pragma region ShaderProperty

HashCode ShaderProperty::GetHashCode() const
{
    HashCode hc = name.GetHashCode();

    if (HasValue())
    {
        hc.Add(GetValueString().GetHashCode());
    }

    // @NOTE: enum values aren't part of the hash code in order to allow
    // changing selecting shader via name / value hash

    return hc;
}

String ShaderProperty::ToString() const
{
    if (IsValueGroup())
    {
        return HYP_FORMAT("{}({})", name, enumValues.Size());
    }
    else if (IsPermutable())
    {
        return HYP_FORMAT("{}(*)", name);
    }
    else if (IsStatic())
    {
        if (HasValue())
        {
            return HYP_FORMAT("{}={}", name, GetValueString());
        }

        return *name;
    }

    HYP_UNREACHABLE();
}

String ShaderProperty::GetValueString() const
{
    if (HasValue())
    {
        String str;

        Visit(currentValue, [&](auto&& value)
            {
                str = HYP_FORMAT("{}", value);
            });

        return str;
    }

    return String::empty;
}

#pragma endregion ShaderProperty

#pragma region ShaderVariantPerms

ShaderVariantPerms& ShaderVariantPerms::Set(const ShaderProperty& property, bool enabled)
{
    if (property.IsVertexAttribute())
    {
        const VertexAttribute* attr = nullptr;

        if (!FindVertexAttributeForDefinition(property.GetValueString(), attr))
        {
            HYP_LOG(ShaderCompiler, Error,
                "Invalid vertex attribute name for shader: {}",
                property.GetValueString());

            return *this;
        }

        Assert(attr != nullptr);

        if (property.IsOptionalVertexAttribute())
        {
            if (enabled)
            {
                m_optionalVertexAttributes |= *attr;
                m_optionalVertexAttributes &= ~m_requiredVertexAttributes;
            }
            else
            {
                m_optionalVertexAttributes &= ~(*attr);
            }

            // NOTE: Optional vertex attributes should not trigger any hash code
            // recalculation.

            return *this;
        }

        if (enabled)
        {
            m_requiredVertexAttributes |= *attr;
            m_optionalVertexAttributes &= ~(*attr);
        }
        else
        {
            m_requiredVertexAttributes &= ~(*attr);
        }

        m_needsHashCodeRecalculation = true;

        return *this;
    }

    const auto it = m_props.Find(property);

    if (enabled)
    {
        if (it == m_props.End())
        {
            m_props.Insert(property);

            m_needsHashCodeRecalculation = true;

            return *this;
        }

        if (*it != property)
        {
            *it = property;

            m_needsHashCodeRecalculation = true;
        }

        return *this;
    }

    if (it != m_props.End())
    {
        m_props.Erase(it);

        m_needsHashCodeRecalculation = true;
    }

    return *this;
}

String ShaderVariantPerms::ToString() const
{
    String propertiesString;
    int counter = 0;

    for (const ShaderProperty& property : GetPropertySet())
    {
        propertiesString += property.ToString();

        if (counter++ < GetPropertySet().Size() - 1)
        {
            propertiesString += ", ";
        }
    }

    return propertiesString;
}

#pragma endregion ShaderVariantPerms

#pragma region ShaderPropertySet

Array<ShaderPropertyId> ShaderPropertySet::ToArray() const
{
    Array<ShaderPropertyId> result;
    result.Reserve(ByteUtil::BitCount(chunks[0])
        + ByteUtil::BitCount(chunks[1])
        + ByteUtil::BitCount(chunks[2])
        + ByteUtil::BitCount(chunks[3]));

    uint64 chunkOffset = 0;
    for (uint64 chunk : chunks)
    {
        FOR_EACH_BIT(chunk, bit)
        {
            ShaderPropertyId propertyId = ShaderPropertyId(chunkOffset + bit);
                
            result.PushBack(propertyId);
        }

        chunkOffset += 64;
    }

    return result;
}

String ShaderPropertySet::GetDebugString() const
{
    String str;

    for (ShaderPropertyId propertyId : ToArray())
    {
        if (!str.Empty())
            str += ", ";

        ShaderProperty property;
        if (!GetShaderPropertyById(propertyId, property))
        {
            str += "<UNKNOWN>";
        }

        str += property.ToString();
    }

    return str;
}

#pragma endregion ShaderPropertySet

#pragma region ShaderCompiler

ShaderCompiler::ShaderCompiler()
    : m_definitions(nullptr)
{
#if HYP_GLSLANG
    ShInitialize();
#endif

#if HYP_DXC
    if (!s_dxcUtils)
        DxcCreateInstance(CLSID_DxcUtils, __uuidof(IDxcUtils), &s_dxcUtils);

    if (!s_dxcCompiler)
        DxcCreateInstance(CLSID_DxcCompiler, __uuidof(IDxcCompiler3), &s_dxcCompiler);
#endif
}

ShaderCompiler::~ShaderCompiler()
{
#if HYP_GLSLANG
    ShFinalize();
#endif

#if HYP_DXC
    s_dxcUtils.Reset();
    s_dxcCompiler.Reset();
#endif

    if (m_definitions)
    {
        delete m_definitions;
    }
}

void ShaderCompiler::ParseDefinitionSection(
    const INIFile::Section& section,
    ShaderBundleDecl& outShaderBundleDecl)
{
    for (const auto& sectionIt : section)
    {
        if (sectionIt.first == "versions")
        {
            // set each property
            for (const auto& element : sectionIt.second.elements)
            {
                if (element.subElements.Any())
                {
                    // Add subelements - parse int / float / string values
                    Array<ShaderProperty::Value> enumValues;
                    enumValues.Reserve(element.subElements.Size());

                    for (const String& subElement : element.subElements)
                    {
                        if (subElement.Empty())
                        {
                            HYP_LOG(ShaderCompiler, Warning,
                                "Empty shader property value for property {}",
                                element.name);

                            continue;
                        }

                        ShaderProperty::Value value;

                        if (std::isdigit(subElement.GetChar(0)))
                        {
                            if (subElement.Contains('.'))
                            {
                                float floatValue;

                                if (!StringUtil::Parse(subElement, &floatValue))
                                {
                                    HYP_LOG(ShaderCompiler, Warning,
                                        "Failed to parse shader property value {} as float for property {}",
                                        subElement, element.name);

                                    continue;
                                }

                                value = floatValue;
                            }
                            else
                            {
                                int intValue;

                                if (!StringUtil::Parse(subElement, &intValue))
                                {
                                    HYP_LOG(ShaderCompiler, Warning,
                                        "Failed to parse shader property value {} as integer for property {}",
                                        subElement, element.name);

                                    continue;
                                }

                                value = intValue;
                            }
                        }
                        else
                        {
                            // string value
                            value = CreateNameFromDynamicString(subElement);
                        }

                        AssertDebug(value.IsValid());

                        enumValues.PushBack(std::move(value));
                    }

                    outShaderBundleDecl.variantPerms.AddValueGroup(CreateNameFromDynamicString(*element.name), enumValues);
                }
                else
                {
                    outShaderBundleDecl.variantPerms.AddPermutation(CreateNameFromDynamicString(*element.name));
                }
            }

            continue;
        }

        auto shaderTypeNameIt = s_shaderTypeNames.Find(sectionIt.first.ToLower());

        if (shaderTypeNameIt != s_shaderTypeNames.End())
        {
            outShaderBundleDecl.sources[shaderTypeNameIt->second] = GetResourceDirectory() / "shaders" / sectionIt.second.GetValue().name;

            continue;
        }
            
        HYP_LOG(ShaderCompiler, Warning,
            "Unknown property in shader definition file: {}\n",
            sectionIt.first);
    }
}

bool ShaderCompiler::HandleBundle(
    ShaderBundleDecl& decl,
    Optional<ShaderRequest> shaderRequest,
    const FilePath& outputFilePath,
    ShaderBundle& inOutBundle)
{
    // Check that each version specified is present in the ShaderBundle.
    // OR any src files have been changed since the object file was compiled.
    // if not, we need to recompile those versions.

    // TODO: Each individual item should have a timestamp internally set
    const Time objectFileLastModified = outputFilePath.LastModifiedTimestamp();

    Time maxSourceFileLastModified = Time(0);

    for (const auto& sourceFile : decl.sources)
    {
        maxSourceFileLastModified = MathUtil::Max(maxSourceFileLastModified, FilePath(sourceFile.second).LastModifiedTimestamp());
    }

    if (maxSourceFileLastModified > objectFileLastModified)
    {
        HYP_LOG(ShaderCompiler, Info,
            "Source file in bundle {} has been modified since the bundle was "
            "last compiled, recompiling...",
            *decl.name);

        inOutBundle = ShaderBundle {};

        return CompileBundle(decl, shaderRequest, inOutBundle, !ShouldCompileEntireBundle);
    }

    // find variants for the bundle that are not in the compiled bundle
    Array<ShaderVariantPerms> missingPerms;

    if (ShouldCompileMissingVariants)
    {
        ForEachPermutation(
            decl.variantPerms,
            [&](const ShaderVariantPerms& perm)
            {
                // get hashcode for this permutation
                // only care about the property set (not vertex attributes), as we will
                // only have access to those from the bundle plus, changing vertex
                // attributes will cause a recompile anyway due to shaders' file
                // contents changing
                const HashCode propertySetHashCode = perm.GetPropertySetHashCode();

                const auto it = inOutBundle.compiledShaders.FindIf(
                    [propertySetHashCode](const CompiledShader& item)
                    {
                        return item.propertySetHashCode == propertySetHashCode;
                    });

                if (it == inOutBundle.compiledShaders.End())
                {
                    missingPerms.PushBack(perm);
                }
            },
            false);
    }

    const bool anyMissing = missingPerms.Any();

    const bool requestedFound = shaderRequest.HasValue() && inOutBundle.compiledShaders.FindIf([&shaderRequest](const CompiledShader& compiledShader)
        {
            return SatisfiesRequested(shaderRequest->properties, shaderRequest->vertexAttributes, compiledShader);
        }) != inOutBundle.compiledShaders.End();

    if (anyMissing || (shaderRequest.HasValue() && !requestedFound))
    {
        String missingPermsString;

        if (anyMissing)
        {
            SizeType index = 0;

            for (const ShaderVariantPerms& perm : missingPerms)
            {
                missingPermsString += String::ToString(perm.GetPropertySetHashCode().Value())
                    + " - " + perm.ToString()
                    + " - " + (perm.GetRequiredVertexAttributes() ? perm.GetRequiredVertexAttributes().ToString() : "<no vertex attributes>");

                if (index != missingPerms.Size() - 1)
                {
                    missingPermsString += ",\n\t";
                }

                index++;
            }
        }

        if (CanCompileShaders())
        {
            return CompileBundle(
                decl,
                shaderRequest,
                inOutBundle,
                !ShouldCompileEntireBundle);
        }

        return false;
    }

    return true;
}

bool ShaderCompiler::LoadBundle(
    Name name,
    Optional<ShaderRequest> shaderRequest,
    ShaderBundle& bundle)
{
    if (!CanCompileShaders())
    {
        HYP_LOG(ShaderCompiler, Warning,
            "Not compiled with shader compilation support... Shaders may become out of date.\n"
            "If any shader bundle files are missing, they will not be compiled on the fly.");
    }

    if (!m_definitions || !m_definitions->IsValid())
    {
        // load for first time if no definitions loaded
        if (!LoadShaderDefinitions())
        {
            return false;
        }
    }

    const String nameString = *name;

    if (!m_definitions->HasSection(nameString))
    {
        // not in definitions file
        HYP_LOG(ShaderCompiler, Error,
            "Section {} not found in shader definitions file", name);

        return false;
    }

    ShaderBundleDecl decl { name };
    MergeGlobalShaderProperties(decl.variantPerms);

    // apply each permutable property from the definitions file
    const INIFile::Section& section = m_definitions->GetSection(nameString);
    ParseDefinitionSection(section, decl);

    auto ForceRecompile = [&](const FilePath& outputFilePath)
    {
        if (CanCompileShaders())
        {
            HYP_LOG(ShaderCompiler, Info, "Attempting to compile shader {}...",
                outputFilePath);
        }
        else
        {
            HYP_LOG(ShaderCompiler, Error,
                "Failed to load compiled shader file: {}. The file could not be "
                "found.",
                outputFilePath);

            return false;
        }

        if (!CompileBundle(decl, shaderRequest, bundle, !ShouldCompileEntireBundle))
        {
            HYP_LOG(ShaderCompiler, Error, "Failed to compile shader bundle {}", name);

            return false;
        }

        return LoadBundleFromFilePath(outputFilePath, bundle);
    };

    const FilePath outputFilePath = GetCacheDirectory() / "ShaderBundles" / nameString + ".shaderbundle";

    if (outputFilePath.Exists())
    {
#ifdef HYP_SHADER_COMPILER_LOGGING
        HYP_LOG(ShaderCompiler, Info, "Attempting to load compiled shader {}...", outputFilePath);
#endif

        if (!LoadBundleFromFilePath(outputFilePath, bundle))
        {
            if (!ForceRecompile(outputFilePath))
            {
                return false;
            }
        }
    }
    else if (!ForceRecompile(outputFilePath))
    {
        return false;
    }

    return HandleBundle(decl, shaderRequest, outputFilePath, bundle);
}

bool ShaderCompiler::LoadShaderDefinitions(bool precompileShaders)
{
    if (m_definitions && m_definitions->IsValid())
    {
        return true;
    }

    const FilePath dataPath = GetCacheDirectory() / "ShaderBundles";

    if (!dataPath.Exists())
    {
        if (FileSystem::MkDir(dataPath.Data()) != 0)
        {
            HYP_LOG(ShaderCompiler, Error, "Failed to create data path at {}",
                dataPath);

            return false;
        }
    }

    if (m_definitions)
    {
        delete m_definitions;
    }

    m_definitions = new INIFile(GetResourceDirectory() / "Shaders.ini");

    if (!m_definitions->IsValid())
    {
        HYP_LOG(ShaderCompiler, Warning,
            "Failed to load shader definitions file at path: {}",
            m_definitions->GetFilePath());

        delete m_definitions;
        m_definitions = nullptr;

        return false;
    }

    m_shaderBundleDecls.Clear();
    m_shaderBundleDecls.Reserve(m_definitions->GetSections().Size());

    for (const auto& it : m_definitions->GetSections())
    {
        const String& key = it.first;
        const INIFile::Section& section = it.second;

        const Name nameFromString = CreateNameFromDynamicString(ANSIString(key));

        ShaderBundleDecl& decl = m_shaderBundleDecls.EmplaceBack(nameFromString);
        ParseDefinitionSection(section, decl);
    }

    if (!precompileShaders)
    {
        return true;
    }

    HYP_LOG(ShaderCompiler, Info, "Precompiling shaders...");

    const bool supportsRtShaders = g_renderInterface->GetRenderConfig().rayTracing;

    HashMap<const ShaderBundleDecl*, bool> results;
    Mutex resultsMutex;

    // Compile all shaders ahead of time
    TaskSystem::GetInstance().ParallelForEach(m_shaderBundleDecls, [&](const ShaderBundleDecl& decl, uint32, uint32)
        {
            if (decl.HasRTShaders() && !supportsRtShaders)
            {
                HYP_LOG(ShaderCompiler, Warning,
                    "Not compiling shader {} because it contains ray tracing "
                    "shaders and ray tracing is not supported on this device.",
                    decl.name);

                return;
            }

            // @TODO Just use LoadBundle with empty Optional<ShaderRequest>

            ForEachPermutation(
                decl.variantPerms,
                [&](const ShaderVariantPerms& shaderVariant)
                {
                    ShaderPropertySet properties;
                    for (const ShaderProperty& property : shaderVariant.GetPropertySet())
                    {
                        properties.Add(InternShaderProperty(property));
                    }

                    VertexAttributeSet vertexAttributes;
                    for (const VertexAttribute* attr : shaderVariant.GetRequiredVertexAttributes().BuildAttributes())
                    {
                        vertexAttributes.Set(*attr);
                    }

                    CompiledShader compiledShader;
                    bool result = RequestShader(decl.name, properties, vertexAttributes, compiledShader);

                    Mutex::Guard guard(resultsMutex);
                    results[&decl] = result;
                },
                false); // true);
        });

    bool allResults = true;

    for (const auto& it : results)
    {
        if (!it.second)
        {
            String permutationString;

            HYP_LOG(ShaderCompiler, Error,
                "{}: Loading of compiled shader failed!\n\tProperties: {}\n\tAttributes: {}",
                it.first->name, it.first->variantPerms.ToString(), it.first->variantPerms.GetRequiredVertexAttributes().ToString());

            allResults = false;
        }
    }

    return allResults;
}

bool ShaderCompiler::CanCompileShaders() const
{
#if HYP_GLSLANG || HYP_DXC
    return true;
#else
    return false;
#endif
}

// Hyperion-specific custom preprocessor directives

static String ExtractFirstToken(const String& str)
{
    String result;

    for (SizeType i = 0; i < str.Size(); i++)
    {
        const char ch = str.Data()[i];

        if (std::isalnum(ch) || ch == '_')
        {
            result.Append(ch);
        }
        else
        {
            break;
        }
    }

    return result;
}

static bool MatchesAnyToken(const String& token, std::initializer_list<const char*> keywords)
{
    for (const char* keyword : keywords)
    {
        if (token == keyword)
        {
            return true;
        }
    }

    return false;
}

static TResult<ShaderInputType> ParseDescriptorTypeFromDeclaration(ShaderLanguage language, const String& declaration, EnumFlags<DescriptorUsageFlags> flags)
{
    const String trimmed = declaration.TrimmedLeft();
    const String firstToken = ExtractFirstToken(trimmed);

    auto MakeUniformBufferType = [flags]() -> ShaderInputType
    {
        return (flags & DescriptorUsageFlags::DYNAMIC)
            ? ShaderInputType::UNIFORM_BUFFER_DYNAMIC
            : ShaderInputType::UNIFORM_BUFFER;
    };

    auto MakeStorageBufferType = [flags]() -> ShaderInputType
    {
        return (flags & DescriptorUsageFlags::DYNAMIC)
            ? ShaderInputType::STORAGE_BUFFER_DYNAMIC
            : ShaderInputType::STORAGE_BUFFER;
    };

    if (language == ShaderLanguage::HLSL)
    {
        if (MatchesAnyToken(firstToken, { "cbuffer" }))
        {
            return MakeUniformBufferType();
        }

        if (MatchesAnyToken(firstToken, {
            "StructuredBuffer", "RWStructuredBuffer",
            "ByteAddressBuffer", "RWByteAddressBuffer",
            "AppendStructuredBuffer", "ConsumeStructuredBuffer",
            "Buffer", "RWBuffer" }))
        {
            return MakeStorageBufferType();
        }

        if (MatchesAnyToken(firstToken, {
            "RWTexture1D", "RWTexture2D", "RWTexture3D",
            "RWTexture1DArray", "RWTexture2DArray" }))
        {
            return ShaderInputType::IMAGE_STORAGE;
        }

        if (MatchesAnyToken(firstToken, {
            "Texture1D", "Texture2D", "Texture3D",
            "TextureCube", "Texture1DArray", "Texture2DArray", "TextureCubeArray" }))
        {
            return ShaderInputType::IMAGE;
        }

        if (MatchesAnyToken(firstToken, { "SamplerState", "SamplerComparisonState", "sampler" }))
        {
            return ShaderInputType::SAMPLER;
        }

        if (firstToken == "RaytracingAccelerationStructure")
        {
            return ShaderInputType::TLAS;
        }

        return HYP_MAKE_ERROR(Error, "Unable to determine descriptor type from HLSL declaration: '{}'", trimmed);
    }
    else // GLSL
    {
        // buffer with explicit r/w semantics
        if (MatchesAnyToken(firstToken, { "readonly", "writeonly", "coherent", "volatile" }))
        {
            const String remaining = String(trimmed.Substr(firstToken.Length())).TrimmedLeft();
            const String secondToken = ExtractFirstToken(remaining);

            if (secondToken == "buffer")
            {
                return MakeStorageBufferType();
            }

            return HYP_MAKE_ERROR(Error, "Unable to determine descriptor type from GLSL declaration: '{}'", trimmed);
        }

        // r/w buffer
        if (firstToken == "buffer")
        {
            return MakeStorageBufferType();
        }
        
        if (firstToken == "uniform")
        {
            const String remaining = String(trimmed.Substr(firstToken.Length())).TrimmedLeft();
            const String secondToken = ExtractFirstToken(remaining);

            // uniform sampler
            if (secondToken.StartsWith("sampler"))
            {
                return ShaderInputType::SAMPLER;
            }
            
            // uniform texture (sampled image)
            if (MatchesAnyToken(secondToken, {
                "texture1D", "texture2D", "texture3D", "textureCube",
                "texture1DArray", "texture2DArray", "textureCubeArray",
                "textureBuffer",
                "itexture1D", "itexture2D", "itexture3D", "itextureCube",
                "itextureBuffer",
                "utexture1D", "utexture2D", "utexture3D", "utextureCube",
                "utextureBuffer" }))
            {
                return ShaderInputType::IMAGE;
            }
            
            // uniform (write/read)only image (storage image)
            if (MatchesAnyToken(secondToken, { "readonly", "writeonly" }))
            {
                const String thirdToken = ExtractFirstToken(String(remaining.Substr(secondToken.Length())).TrimmedLeft());

                if (thirdToken.StartsWith("image")
                    || thirdToken.StartsWith("iimage")
                    || thirdToken.StartsWith("uimage"))
                {
                    return ShaderInputType::IMAGE_STORAGE;
                }

                return HYP_MAKE_ERROR(Error, "Unable to determine descriptor type from GLSL declaration: '{}'", trimmed);
            }
            
            // uniform image
            if (MatchesAnyToken(secondToken, {
                "image1D", "image2D", "image3D",
                "imageCube", "image1DArray", "image2DArray", "imageCubeArray",
                "imageBuffer",
                "iimage1D", "iimage2D", "iimage3D",
                "iimageBuffer", "iimageCube",
                "uimage1D", "uimage2D", "uimage3D",
                "uimageBuffer", "uimageCube" }))
            {
                return ShaderInputType::IMAGE_STORAGE;
            }

            // uniform accelerationStructureEXT
            if (secondToken == "accelerationStructureEXT")
            {
                return ShaderInputType::TLAS;
            }

            // uniform [StructName] - contant buffer
            return MakeUniformBufferType();
        }

        return HYP_MAKE_ERROR(Error, "Unable to determine descriptor type from GLSL declaration: '{}'", trimmed);
    }
}

static String FormatDescriptorDeclaration(
    ShaderLanguage language,
    const DescriptorUsage& usage,
    const String& setName,
    const String& descriptorName,
    const String& stdVersion,
    const Array<String>& additionalParams,
    const String& declarationBody)
{
    if (language == ShaderLanguage::HLSL)
    {
        String remaining = declarationBody.Trimmed();
        SizeType insertPos = remaining.FindFirstIndex(";");
        
        if (insertPos == String::NotFound)
        {
            insertPos = remaining.FindFirstIndex("{");
        }
        
        String declaration = (insertPos == String::NotFound) ? remaining : String(remaining.Substr(0, insertPos));
        String suffix = (insertPos == String::NotFound) ? String::empty : String(remaining.Substr(insertPos));

        return HYP_FORMAT("{} : register(_{}_{}_REGISTER, _{}_SPACE) {}\n",
             declaration,
             setName, descriptorName,
             setName,
             suffix);
    }
    else
    {
        String decl = "layout(";
        
        if (usage.IsBuffer())
        {
             decl += stdVersion + ", ";
        }

        decl += "set=_" + setName + "_SET" + ", binding=_" + setName + "_" + descriptorName + "_BINDING";

        if (additionalParams.Any())
        {
            decl += ", " + String::Join(additionalParams, ", ");
        }

        return decl + ") " + declarationBody + "\n";
    }
}

ShaderCompiler::ProcessResult ShaderCompiler::ProcessShaderSource(
    ProcessShaderSourcePhase phase,
    ShaderModuleType type,
    ShaderLanguage language, const String& source, const String& filename,
    const ShaderVariantPerms& perm)
{
    ProcessResult result;
    Array<String> lines;

    if (phase == ProcessShaderSourcePhase::AFTER_PREPROCESS)
    {
        String preprocessedSource;
        Array<String> preprocessErrorMessages;
    
        bool preprocessResult = false;
        
        const String preamble = BuildAttributesDefines(language, perm);
        
        if (language == ShaderLanguage::GLSL)
        {
#if HYP_GLSLANG
            preprocessResult = PreprocessGLSL(
                type,
                preamble,
                source,
                filename,
                preprocessedSource,
                preprocessErrorMessages);
#else
            preprocessErrorMessages.PushBack("GLSL preprocessing not supported in this build.");
            preprocessResult = false;
#endif
        }
        else
        {
            preprocessResult = PreprocessHLSL(
                type,
                preamble,
                source,
                filename,
                preprocessedSource,
                preprocessErrorMessages);
        }

        result.errors.Concat(Map(preprocessErrorMessages, [](const String& errorMessage)
            {
                return ProcessError { errorMessage };
            }));

        if (!preprocessResult)
        {
            return result;
        }

        lines = preprocessedSource.Split('\n');
    }
    else
    {
        lines = source.Split('\n');
    }

    struct ParseCustomStatementResult
    {
        Array<String> args;
        String remaining;
    };

    auto ParseCustomStatement = [](const String& start, const String& line) -> ParseCustomStatementResult
    {
        const String substr = line.Substr(start.Length());

        String argsString;

        int parenthesesDepth = 0;
        SizeType index;

        // Note: using 'Size' and 'Data' to index -- not using utf-8 chars here.
        for (index = 0; index < substr.Size(); index++)
        {
            if (substr.Data()[index] == ')')
            {
                parenthesesDepth--;
            }

            if (parenthesesDepth > 0)
            {
                argsString.Append(substr.Data()[index]);
            }

            if (substr.Data()[index] == '(')
            {
                parenthesesDepth++;
            }

            if (parenthesesDepth <= 0)
            {
                break;
            }
        }

        Array<String> args = argsString.Split(',');

        for (String& arg : args)
        {
            arg = arg.Trimmed();
        }

        return { std::move(args), substr.Substr(index + 1) };
    };

    int lastAttributeLocation = -1;

    for (uint32 lineIndex = 0; lineIndex < lines.Size();)
    {
        const String line = lines[lineIndex++].Trimmed();

        switch (phase)
        {
        case ProcessShaderSourcePhase::BEFORE_PREPROCESS:
        {
            if (type == ShaderModuleType::Vertex && line.StartsWith("HYP_ATTRIBUTE"))
            {
                Array<String> parts = line.Split(' ');

                bool optional = false;

                if (parts.Size() < 3)
                {
                    result.errors.PushBack(ProcessError { "Invalid attribute:  Requires format HYP_ATTRIBUTE(location) type name" });

                    break;
                }

                char ch;

                String attributeKeyword;
                String attributeLocation;

                Optional<String> attributeCondition;
                
                SizeType attrStringIndex = 0;

                {
                    while (attrStringIndex != parts.Front().Size() && (std::isalpha(ch = parts.Front()[attrStringIndex]) || ch == '_'))
                    {
                        attributeKeyword.Append(ch);
                        ++attrStringIndex;
                    }

                    if (attributeKeyword == "HYP_ATTRIBUTE_OPTIONAL")
                    {
                        optional = true;
                    }
                    else if (attributeKeyword == "HYP_ATTRIBUTE")
                    {
                        optional = false;
                    }
                    else
                    {
                        result.errors.PushBack(ProcessError { String("Invalid attribute, unknown attribute keyword `") + attributeKeyword + "`" });

                        break;
                    }

                    if (attrStringIndex != parts.Front().Size() && ((ch = parts.Front()[attrStringIndex]) == '('))
                    {
                        ++attrStringIndex;

                        // read integer string
                        while (attrStringIndex != parts.Front().Size() && std::isdigit(ch = parts.Front()[attrStringIndex]))
                        {
                            attributeLocation.Append(ch);
                            ++attrStringIndex;
                        }

                        // if there is a comma, read the conditional define that we will use
                        if (attrStringIndex != parts.Front().Size() && ((ch = parts.Front()[attrStringIndex]) == ','))
                        {
                            ++attrStringIndex;

                            String condition;
                            while (attrStringIndex != parts.Front().Size() && (std::isalpha(ch = parts.Front()[attrStringIndex]) || ch == '_'))
                            {
                                condition.Append(ch);
                                ++attrStringIndex;
                            }

                            attributeCondition = condition;
                        }

                        if (attrStringIndex != parts.Front().Size() && ((ch = parts.Front()[attrStringIndex]) == ')'))
                        {
                            ++attrStringIndex;
                        }
                        else
                        {
                            result.errors.PushBack(ProcessError { "Invalid attribute, missing closing parenthesis" });

                            break;
                        }

                        if (attributeLocation.Empty())
                        {
                            result.errors.PushBack(ProcessError { "Invalid attribute location" });

                            break;
                        }
                    }
                }

                const String remaining = line.Substr(attrStringIndex);

                auto IsIdentiferChar = [](utf::Char32 ch)
                {
                    return std::isalnum(utf::Char32(ch)) || ch == utf::Char32('_');
                };

                VertexAttributeDefinition attributeDefinition {};
                attributeDefinition.name = StringUtil::TakeWhile(parts[2], IsIdentiferChar);
                attributeDefinition.typeClass = StringUtil::TakeWhile(parts[1], IsIdentiferChar);
                attributeDefinition.location = attributeLocation.Any()
                    ? std::atoi(attributeLocation.Data())
                    : lastAttributeLocation + 1;

                lastAttributeLocation = attributeDefinition.location;

                if (optional)
                {
                    result.optionalAttributes.PushBack(attributeDefinition);

                    if (attributeCondition.HasValue())
                    {
                        result.processedSource += "#if defined(" + attributeCondition.Get() + ") && " + attributeCondition.Get() + "\n";

                        attributeDefinition.condition = *attributeCondition;
                    }
                    else
                    {
                        result.processedSource += "#ifdef HYP_ATTRIBUTE_" + attributeDefinition.name + "\n";
                    }
                }
                else
                {
                    result.requiredAttributes.PushBack(attributeDefinition);
                }

                if (language == ShaderLanguage::GLSL)
                {
                    result.processedSource += "layout(location=" + String::ToString(attributeDefinition.location) + ") in " + attributeDefinition.typeClass + " " + attributeDefinition.name + ";\n";
                }
                else if (language == ShaderLanguage::HLSL)
                {
                    // For hlsl we just paste have the rest of the line after the macro invocation, e.g:
                    // HYP_ATTRIBUTE(5) float3 bitangent : BINORMAL;
                    result.processedSource += remaining + '\n';
                }

                if (optional)
                {
                    result.processedSource += "#endif\n";
                }

                continue;
            }

            break;
        }
        case ProcessShaderSourcePhase::AFTER_PREPROCESS:
        {
            if (line.StartsWith("DECLARE"))
            {
                String commandStr;

                for (SizeType index = 0; index < line.Size(); index++)
                {
                    if (std::isalnum(line.Data()[index]) || line.Data()[index] == '_')
                    {
                        commandStr.Append(line.Data()[index]);
                    }
                    else
                    {
                        break;
                    }
                }

                ShaderRegister slot = ShaderRegister::NONE;
                EnumFlags<DescriptorUsageFlags> flags = DescriptorUsageFlags::NONE;

                if (commandStr == "DECLARE_SRV" || commandStr == "DECLARE_SRV_DYNAMIC")
                {
                    slot = ShaderRegister::SRV;
                }
                else if (commandStr == "DECLARE_UAV" || commandStr == "DECLARE_UAV_DYNAMIC")
                {
                    slot = ShaderRegister::UAV;
                }
                else if (commandStr == "DECLARE_BUFFER" || commandStr == "DECLARE_BUFFER_DYNAMIC")
                {
                    slot = ShaderRegister::BUFFER;
                }
                else if (commandStr == "DECLARE_ACCELERATION_STRUCTURE")
                {
                    slot = ShaderRegister::SRV;
                }
                else if (commandStr == "DECLARE_SAMPLER")
                {
                    slot = ShaderRegister::SAMPLER;
                }
                else
                {
                    result.errors.PushBack(ProcessError {
                        "Invalid descriptor slot. Must match DECLARE_<Type> " });

                    break;
                }

                if (commandStr.EndsWith("_DYNAMIC"))
                {
                    flags |= DescriptorUsageFlags::DYNAMIC;
                }

                Array<String> parts = line.Split(' ');

                String descriptorName, setName, slotStr;

                HashMap<String, String> params;

                auto parseResult = ParseCustomStatement(commandStr, line);

                if (parseResult.args.Size() < 2)
                {
                    result.errors.PushBack(ProcessError {
                        "Invalid descriptor: Requires format "
                        "DECLARE_<SLOT>(set, name)" });

                    break;
                }

                setName = parseResult.args[0];
                descriptorName = parseResult.args[1];

                if (parseResult.args.Size() > 2)
                {
                    for (SizeType index = 2; index < parseResult.args.Size(); index++)
                    {
                        Array<String> split = parseResult.args[index].Split('=');

                        for (String& part : split)
                        {
                            part = part.Trimmed();
                        }

                        if (split.Size() != 2)
                        {
                            result.errors.PushBack(ProcessError {
                                "Invalid parameter: Requires format <key>=<value>" });

                            break;
                        }

                        const String& key = split[0];
                        const String& value = split[1];

                        params[key] = value;
                    }
                }
                
                TResult<ShaderInputType> descriptorTypeResult = ParseDescriptorTypeFromDeclaration(language, parseResult.remaining, flags);

                if (!descriptorTypeResult)
                {
                    result.errors.PushBack(ProcessError { descriptorTypeResult.GetError().GetMessage() });

                    continue;
                }

                DescriptorUsage usage {};
                usage.slot = slot;
                usage.type = descriptorTypeResult.GetValue();
                usage.setName = CreateNameFromDynamicString(ANSIString(setName));
                usage.descriptorName = CreateNameFromDynamicString(ANSIString(descriptorName));
                usage.flags = flags;
                usage.params = std::move(params);

                Array<String> additionalParams;
                String stdVersion;
                
                if (language == ShaderLanguage::GLSL)
                {
                    stdVersion = "std140";

                    if (usage.params.Contains("standard"))
                    {
                        stdVersion = usage.params.At("standard");
                    }

                    if (usage.params.Contains("format"))
                    {
                        additionalParams.PushBack(usage.params.At("format"));
                    }

                    if (usage.IsBuffer())
                    {
                        if (usage.params.Contains("matrix_mode"))
                        {
                            additionalParams.PushBack(usage.params.At("matrix_mode"));
                        }
                        else
                        {
                            additionalParams.PushBack("row_major");
                        }
                    }
                }
                
                result.processedSource += FormatDescriptorDeclaration(
                    language, usage, setName, descriptorName, stdVersion,
                    additionalParams, parseResult.remaining);

                result.descriptorUsages.PushBack(usage);

                continue;
            }

            break;
        }
        }

        result.processedSource += line + '\n';
    }

#ifdef HYP_SHADER_COMPILER_LOGGING
    HYP_LOG(ShaderCompiler, Info, "Processed source: {}", result.processedSource);
#endif

    return result;
}

bool ShaderCompiler::CompileBundle(
    const ShaderBundleDecl& decl,
    Optional<ShaderRequest> shaderRequest,
    ShaderBundle& out,
    bool onlyCompileRequested)
{
    if (!CanCompileShaders())
    {
        return false;
    }

    Array<LoadedSourceFile> loadedSourceFiles;
    loadedSourceFiles.Resize(decl.sources.Size());

    Array<Array<ProcessError>> processErrors;
    processErrors.Resize(decl.sources.Size());

    Array<Array<VertexAttributeDefinition>> requiredVertexAttributes;
    requiredVertexAttributes.Resize(decl.sources.Size());

    Array<Array<VertexAttributeDefinition>> optionalVertexAttributes;
    optionalVertexAttributes.Resize(decl.sources.Size());

    TaskBatch taskBatch;

    for (SizeType index = 0; index < decl.sources.Size(); index++)
    {
        StaticMessage debugName;
        debugName.value = ANSIStringView(*decl.sources.AtIndex(index).second);

        taskBatch.AddTask([this, index, &decl, &loadedSourceFiles, &processErrors,
                              &requiredVertexAttributes,
                              &optionalVertexAttributes](...)
            {
                const auto& pair = decl.sources.AtIndex(index);

                const ShaderModuleType moduleType = pair.first;
                const FilePath filepath = pair.second;

                const ShaderLanguage language = filepath.EndsWith("hlsl")
                    ? ShaderLanguage::HLSL
                    : ShaderLanguage::GLSL;

                if (!filepath.Exists())
                {
                    processErrors[index] = {
                        ProcessError { "Shader source file does not exist" }
                    };

                    return;
                }

                FileBufferedReaderSource filepathSource { filepath };
                BufferedReader reader { &filepathSource };

                if (!reader.IsOpen())
                {
                    processErrors[index] = { ProcessError { HYP_FORMAT("Failed to open shader source file: {}", std::strerror(errno)) } };

                    return;
                }

                const ByteBuffer byteBuffer = reader.ReadBytes();

                // we add this define to prevent the DECLARE_* macros from being defines in shader code
                // and folding to nothing.
                String preamble = "#define HYP_SHADER_COMPILER 1\n\n"
                    + HYP_FORMAT("#define {} 1\n\n", ShaderModuleTypeNames[uint8(moduleType)])
                    + HYP_FORMAT("#define LANG_{} 1\n\n", (language == ShaderLanguage::HLSL ? "HLSL" : "GLSL"));

                String sourceString = String(byteBuffer.ToByteView()).ReplaceAll("\r\n", "\n");

                if (language == ShaderLanguage::GLSL)
                {
                    String sourceWithoutVersion;
                    String shaderVersion = GetShaderVersionFromSource(sourceString, sourceWithoutVersion);

                    auto split = sourceString.Split('\n');
                    auto splitIt = split.Find(shaderVersion);

                    if (splitIt != split.End())
                    {
                        const SizeType versionIndex = splitIt - split.Begin();

                        preamble += String("#line ") + String::ToString(versionIndex + 1) + "\n";
                    }
                    else
                    {
                        preamble += "#line 1\n";
                    }

                    // #version must occur first in shader (GLSL)
                    sourceString = shaderVersion + "\n"
                        + preamble
                        + sourceWithoutVersion;
                }
                else
                {
                    preamble += "#line 1\n\n";
                    sourceString = preamble + sourceString;
                }

                // process shader source to extract vertex attributes.
                // runs before actual preprocessing
                ProcessResult result = ProcessShaderSource(
                    ProcessShaderSourcePhase::BEFORE_PREPROCESS,
                    pair.first,
                    language,
                    sourceString,
                    filepath,
                    {});

                if (result.errors.Any())
                {
                    HYP_LOG(ShaderCompiler, Error, "{} shader processing errors!", result.errors.Size());

                    processErrors[index] = result.errors;

                    return;
                }

                requiredVertexAttributes[index] = result.requiredAttributes;
                optionalVertexAttributes[index] = result.optionalAttributes;

                loadedSourceFiles[index] = LoadedSourceFile {
                    .type = pair.first,
                    .language = language,
                    .file = pair.second,
                    .lastModifiedTimestamp = filepath.LastModifiedTimestamp(),
                    .source = result.processedSource
                };
            });
    }

    if (IsOnThread(ThreadCategory::THREAD_CATEGORY_TASK))
    {
        // run on this thread if we are already in a task thread
        taskBatch.ExecuteBlocking();
    }
    else
    {
        // Hack fix: task threads that are currently enqueueing RenderCommands can
        // cause a deadlock, if we are waiting on tasks to complete from the render
        // thread.

        if (IsOnThread(g_renderThread))
        {
            taskBatch.ExecuteBlocking();
        }
        else
        {
            TaskSystem::GetInstance().EnqueueBatch(&taskBatch);
            taskBatch.AwaitCompletion();
        }
    }

    Array<ProcessError> allProcessErrors;

    for (const auto& errorList : processErrors)
    {
        allProcessErrors.Concat(errorList);
    }

    if (!allProcessErrors.Empty())
    {
        for (const ProcessError& error : allProcessErrors)
        {
            HYP_LOG(ShaderCompiler, Error, "\t{}", error.errorMessage);
        }

        HYP_BREAKPOINT;

        return false;
    }

    // grab each defined property, and iterate over each combination
    ShaderVariantPerms permsToCompile;
    MergeGlobalShaderProperties(permsToCompile);

    if (!onlyCompileRequested)
    {
        permsToCompile.Merge(decl.variantPerms);
    }

    { // Lookup vertex attribute names

        VertexAttributeSet requiredVertexAttributeSet;
        VertexAttributeSet optionalVertexAttributeSet;

        for (const Array<VertexAttributeDefinition>& definitions : requiredVertexAttributes)
        {
            requiredVertexAttributeSet |= BuildVertexAttributeSet(definitions);
        }

        for (const Array<VertexAttributeDefinition>& definitions : optionalVertexAttributes)
        {
            optionalVertexAttributeSet |= BuildVertexAttributeSet(definitions);
        }

        permsToCompile.SetRequiredVertexAttributes(requiredVertexAttributeSet);
        permsToCompile.SetOptionalVertexAttributes(optionalVertexAttributeSet);
    }

    // INFO ON MERGING 'ADDITIONAL' SHADER VERSIONS (upon requesting a shader)
    // ============================================
    // if shaderRequest is set, we need to properly merge those properties with our ShaderVariant.
    // 
    // We assume all requested properties are NOT permutable.
    // 
    // VALUE GROUPS / ENUMS:
    // If OUTPUT=RGBA8 is added and we have in our ShaderVariant, OUTPUT={RGBA8, RGBA16F}, we need to add
    // RGBA8 to the existing `OUTPUT` ValueGroup rather than applying it to EVERY single permutation.
    // =============================================
    if (shaderRequest.HasValue())
    {
        const auto MergeProperty = [](ShaderVariantPerms& target, const ShaderProperty& additional) -> Result
        {
            if (additional.IsPermutable())
            {
                return HYP_MAKE_ERROR(Error, "Requested shader with permutable property {} (which is not allowed)", additional.name);
            }

            auto targetIt = target.Find(StringHash(additional.name));

            if (additional.HasValue())
            {
                // Find ValueGroup in target with same name
                if (targetIt != target.End())
                {
                    if (targetIt->IsValueGroup())
                    {
                        // Add each value from additional to the target's ValueGroup
                        targetIt->AddEnumValue(additional.currentValue);

                        return {};
                    }

                    if (*targetIt == additional)
                    {
                        // already exists but equal; no need to add or give an error.
                        return {};
                    }

                    // conflict: trying to add a value to a non-ValueGroup property
                    return HYP_MAKE_ERROR(Error, "Duplicate property: {} already exists and is not a ValueGroup we can append to!", additional.name);
                }
                else
                {
                    Array<ShaderProperty::Value> valueArray(1);
                    valueArray[0] = additional.currentValue;

                    // Add new ValueGroup to the shader variant.
                    target.AddValueGroup(additional.name, valueArray);

                    return {};
                }
            }

            if (targetIt != target.End() && *targetIt == additional)
            {
                // already exists but equal; no need to add or give duplication error.
                return {};
            }

            if (additional.IsValueGroup())
            {
                if (targetIt != target.End())
                {
                    // merge each value from additional to the target's ValueGroup
                    if (targetIt->IsValueGroup())
                    {
                        for (const ShaderProperty::Value& enumValue : additional.enumValues)
                        {
                            targetIt->AddEnumValue(enumValue);
                        }

                        return {};
                    }

                    // conflict: trying to add a ValueGroup to a non-ValueGroup property
                    return HYP_MAKE_ERROR(Error, "Duplicate property: {} already exists and is not a ValueGroup we can merge with!", additional.name);
                }
            }

            target.AddStatic(additional.name);

            return {};
        };

        Array<ShaderProperty> additionalProperties;

        for (ShaderPropertyId propertyId : shaderRequest->properties.ToArray())
        {
            ShaderProperty property;
            if (!GetShaderPropertyById(propertyId, property))
            {
                HYP_LOG(ShaderCompiler, Error, "Failed to find ShaderProperty with ID {} in reverse lookup map!", uint32(propertyId));

                return false;
            }

            additionalProperties.PushBack(std::move(property));
        }

        for (const ShaderProperty& additionalProperty : additionalProperties)
        {
            if (Result mergeResult = MergeProperty(permsToCompile, additionalProperty); mergeResult.HasError())
            {
                HYP_LOG(ShaderCompiler, Warning,
                    "Failed to merge additional shader property {} into final properties: {}",
                    additionalProperty.name,
                    mergeResult.GetError().GetMessage());

                HYP_BREAKPOINT_DEBUG_MODE;
            }
        }
    }

    Mutex compiledShadersMutex;
    Mutex errorMessagesMutex;

    AtomicVar<uint32> numCompiledPermutations { 0u };
    AtomicVar<uint32> numErroredPermutations { 0u };

    // compile shader with each permutation of properties
    ForEachPermutation(
        permsToCompile,
        [&](const ShaderVariantPerms& perm)
        {
            HYP_LOG(ShaderCompiler, Info, "Compiling shader {}\n\tProperties: {}\n\tAttributes: {}",
                decl.name,
                perm.ToString(),
                perm.GetRequiredVertexAttributes().ToString());

            CompiledShader compiledShader;
            compiledShader.name = decl.name;
            
            for (const ShaderProperty& shaderProperty : perm.GetPropertySet())
            {
                const ShaderPropertyId propertyId = InternShaderProperty(shaderProperty);
                compiledShader.properties.Add(propertyId);
            }

            compiledShader.vertexAttributes = perm.GetRequiredVertexAttributes();
            compiledShader.propertySetHashCode = perm.GetPropertySetHashCode();

            uint32 numErrored = 0;
            uint32 numCompiled = 0;

            Array<DescriptorUsageSet> descriptorUsageSetsPerFile;
            descriptorUsageSetsPerFile.Resize(loadedSourceFiles.Size());

            Array<String> processedSources;
            processedSources.Resize(loadedSourceFiles.Size());

            Array<Pair<FilePath, bool /* skip */>> filepaths;
            filepaths.Resize(loadedSourceFiles.Size());

            // load each source file, check if the output file exists, and if it
            // does, check if it is older than the source file if it is, we can
            // reuse the file. otherwise, we process the source and prepare it for
            // compilation
            for (SizeType index = 0; index < loadedSourceFiles.Size(); index++)
            {
                const LoadedSourceFile& item = loadedSourceFiles[index];

                // check if a file exists w/ same hash
                const FilePath outputFilepath = item.GetOutputFilepath(compiledShader);

                filepaths[index] = { outputFilepath, false };

                DescriptorUsageSet& descriptorUsages = descriptorUsageSetsPerFile[index];

                Array<String> errorMessages;

                // set directory to the directory of the shader
                const FilePath dir = GetResourceDirectory() / FilePath::Relative(FilePath(item.file).BasePath(), GetResourceDirectory());

                String& processedSource = processedSources[index];

                { // Process shader (preprocessing, custom statements, etc.)
                    ProcessResult processResult = ProcessShaderSource(
                        ProcessShaderSourcePhase::AFTER_PREPROCESS,
                        item.type,
                        item.language,
                        item.source,
                        item.file,
                        perm);

                    if (processResult.errors.Any())
                    {
                        HYP_LOG(ShaderCompiler, Error, "{} shader processing errors:", processResult.errors.Size());

                        for (const ProcessError& processError : processResult.errors)
                        {
                            HYP_LOG(ShaderCompiler, Error, "\t{}", processError.errorMessage);
                        }

                        Mutex::Guard guard(errorMessagesMutex);
                        out.errorMessages.Concat(Map(processResult.errors, &ProcessError::errorMessage));

                        ++numErrored;

                        continue;
                    }

                    descriptorUsages.Merge(std::move(processResult.descriptorUsages));

                    processedSource = processResult.processedSource;
                }
            }

            // merge all descriptor usages together for the source files before
            // compiling.
            DescriptorUsageSet descriptorUsageSetsMerged;

            for (const DescriptorUsageSet& descriptorUsageSet : descriptorUsageSetsPerFile)
            {
                descriptorUsageSetsMerged.Merge(descriptorUsageSet);
            }

            descriptorUsageSetsPerFile.Clear();

            // final substitution of properties + compilation
            for (SizeType index = 0; index < loadedSourceFiles.Size(); index++)
            {
                const LoadedSourceFile& item = loadedSourceFiles[index];

                const Pair<FilePath, bool>& filepathState = filepaths[index];

                // don't process these files
                if (filepathState.second)
                {
                    continue;
                }

                const FilePath& outputFilepath = filepathState.first;

                Array<String> errorMessages;

                { // logging stuff
                    String variablePropertiesString;
                    String staticPropertiesString;

                    for (const ShaderProperty& property : perm.ToArray())
                    {
                        if (property.IsPermutable())
                        {
                            if (!variablePropertiesString.Empty())
                            {
                                variablePropertiesString += ", ";
                            }

                            variablePropertiesString += property.name.LookupString();
                        }
                        else
                        {
                            if (!staticPropertiesString.Empty())
                            {
                                staticPropertiesString += ", ";
                            }

                            staticPropertiesString += property.name.LookupString();
                        }
                    }

                    // HYP_LOG(
                    //     ShaderCompiler,
                    //     Info,
                    //     "Compiling shader {}\n\tVariable properties: [{}]\n\tStatic
                    //     properties: [{}]\n\tProperties hash: {}", outputFilepath,
                    //     variablePropertiesString,
                    //     staticPropertiesString,
                    //     properties.GetHashCode().Value()
                    // );
                }

                ByteBuffer byteBuffer;
                
                if (item.language == ShaderLanguage::GLSL)
                {
#if HYP_GLSLANG
                    byteBuffer = CompileGLSL(
                        item.type,
                        descriptorUsageSetsMerged,
                        processedSources[index],
                        item.file,
                        errorMessages);

                    if (errorMessages.Any())
                    {
                        Mutex::Guard guard(errorMessagesMutex);
                        out.errorMessages.Concat(errorMessages);
                        
                        ++numErrored;
                        
                        continue;
                    }
#else
                    Mutex::Guard guard(errorMessagesMutex);
                    out.errorMessages.EmplaceBack("Cannot compile GLSL code, glslang not linked");

                    ++numErrored;
                    
                    continue;
#endif
                }

                if (item.language == ShaderLanguage::HLSL)
                {
#if HYP_DXC
                    HLSLOutputType outputType = HLSLOutputType::SPIRV;

#if HYP_VULKAN
                    outputType = HLSLOutputType::SPIRV;
#elif HYP_DX12
                    outputType = HLSLOutputType::DXIL;
#endif

                    byteBuffer = CompileHLSL(
                        item.type,
                        outputType,
                        descriptorUsageSetsMerged,
                        processedSources[index],
                        item.file,
                        perm,
                        errorMessages);

                    if (errorMessages.Any())
                    {
                        Mutex::Guard guard(errorMessagesMutex);
                        out.errorMessages.Concat(errorMessages);
                        
                        ++numErrored;
                        
                        continue;
                    }

#else
                    Mutex::Guard guard(errorMessagesMutex);
                    out.errorMessages.EmplaceBack("Cannot compile HLSL code, DXC not linked");

                    ++numErrored;
                    
                    continue;
#endif
                }

                if (byteBuffer.Empty())
                {
                    Mutex::Guard guard(errorMessagesMutex);
                    out.errorMessages.EmplaceBack("No shader IL returned");

                    ++numErrored;
                    
                    continue;
                }

                { // write the shader bytecode to the temp file
                    FileByteWriter tempWriter(outputFilepath.Data());

                    if (!tempWriter.IsOpen())
                    {
                        Mutex::Guard guard(errorMessagesMutex);
                        out.errorMessages.PushBack(HYP_FORMAT("Could not open file {} for writing!", outputFilepath));

                        ++numErrored;

                        continue;
                    }

                    tempWriter.Write(byteBuffer.Data(), byteBuffer.Size());
                    tempWriter.Close();
                }

                if (item.language == ShaderLanguage::GLSL)
                {
                    // for GLSL, we always have "main" as entry point
                    compiledShader.AddShaderModule(item.type, item.file, "main", std::move(byteBuffer));
                }
                else
                {
                    // for HLSL, we use entry point name based on stage
                    compiledShader.AddShaderModule(item.type, item.file, std::move(byteBuffer));
                }

                ++numCompiled;
            }

            numCompiledPermutations.Increment(numErrored == 0 && numCompiled > 0 ? 1 : 0, MemoryOrder::RELAXED);
            numErroredPermutations.Increment(numErrored > 0 ? 1 : 0, MemoryOrder::RELAXED);

            if (numErrored == 0 && numCompiled > 0)
            {
                compiledShader.inputGroup = ShaderInputGroup();
                descriptorUsageSetsMerged.BuildDescriptorTableDeclaration(compiledShader.inputGroup);

                Mutex::Guard guard(compiledShadersMutex);
                out.compiledShaders.PushBack(std::move(compiledShader));
            }
        },
        false); // true);

    if (out.HasErrors())
    {
        HYP_LOG(ShaderCompiler, Error,
            "Shader compilation failed for shader {} with {} errored permutations!",
            decl.name, numErroredPermutations.Get(MemoryOrder::RELAXED));

        for (const String& errorMessage : out.errorMessages)
        {
            HYP_LOG(ShaderCompiler, Error, "\t{}", errorMessage);
        }

        HYP_BREAKPOINT_DEBUG_MODE;

        return false;
    }

    if (out.compiledShaders.Empty())
    {
        HYP_LOG(ShaderCompiler, Error,
            "No compiled shaders were produced for shader {}",
            decl.name);

        return false;
    }

    // more attributes = higher pri, better fit found first
    std::sort(
        out.compiledShaders.Begin(),
        out.compiledShaders.End(),
        [](const CompiledShader& a, const CompiledShader& b) -> bool
        {
            return ByteUtil::BitCount(a.vertexAttributes.flagMask)
                > ByteUtil::BitCount(b.vertexAttributes.flagMask);
        });

    { // Save the shader property DB
        
        const FilePath shaderPropertyDbPath = GetCacheDirectory() / "ShaderProperties.bin";

        FileByteWriter shaderPropertyDbWriter { shaderPropertyDbPath };
        WriteShaderPropertyDatabase(shaderPropertyDbWriter);
        shaderPropertyDbWriter.Close();
    }

    {
        const FilePath shaderBundleWriterPath = GetCacheDirectory() / "ShaderBundles" / String(*decl.name) + ".shaderbundle";

        FileByteWriter shaderBundleWriter { shaderBundleWriterPath };

        FBOMWriter serializer { FBOMWriterConfig {} };

        if (FBOMResult err = serializer.Append(out))
        {
            HYP_LOG(ShaderCompiler, Error,
                "Failed to serialize compiled shader {}: {}",
                decl.name, err.message);

            HYP_BREAKPOINT_DEBUG_MODE;

            return false;
        }

        if (FBOMResult err = serializer.Emit(&shaderBundleWriter))
        {
            HYP_LOG(ShaderCompiler, Error,
                "Failed to write compiled shader {} to file {}: {}",
                decl.name, shaderBundleWriterPath, err.message);

            HYP_BREAKPOINT_DEBUG_MODE;

            return false;
        }

        shaderBundleWriter.Close();
    }

#ifdef HYP_SHADER_COMPILER_LOGGING
    if (numCompiledPermutations.Get(MemoryOrder::RELAXED) != 0)
    {
        HYP_LOG(ShaderCompiler, Info,
            "Compiled {} new variants for shader {} to: {}",
            numCompiledPermutations.Get(MemoryOrder::RELAXED), decl.name,
            finalOutputPath);
    }
#endif

    return true;
}

bool ShaderCompiler::RequestShader(
    Name name,
    const ShaderPropertySet& properties,
    const VertexAttributeSet& vertexAttributes,
    CompiledShader& out)
{
    ShaderPropertySet mergedProperties = properties;
    MergeGlobalShaderProperties(mergedProperties);

    ShaderBundle bundle;

    if (!LoadBundle(name, ShaderRequest { mergedProperties, vertexAttributes }, bundle))
    {
        HYP_LOG(ShaderCompiler, Error, "Failed to attempt loading of shader bundle: {}", name);

        return false;
    }

    if (bundle.compiledShaders.Empty())
    {
        AssertDebug(false, "Loaded shader bundle has no compiled shaders! Corrupted file?");
        return false;
    }

    // make sure we properly created it
    auto it = bundle.compiledShaders.FindIf(
        [&mergedProperties, &vertexAttributes](const CompiledShader& compiledShader) -> bool
        {
            if (!compiledShader.IsValid())
            {
                HYP_LOG(ShaderCompiler, Error,
                    "Invalid compiled shader found when looking for shader {}",
                    compiledShader.name);

                return false;
            }

            return SatisfiesRequested(mergedProperties, vertexAttributes, compiledShader);
        });

    if (it == bundle.compiledShaders.End())
    {
        HYP_LOG(ShaderCompiler, Error,
            "No match found for requested shader!\n"
            "Name: {}\n"
            "\tRequested properties: {}\n\tVertex Attributes: {}\n\n"
            "Found: {}",
            name, mergedProperties.GetDebugString(), vertexAttributes.ToString(),
            String::Join(bundle.compiledShaders, "\n", [](const CompiledShader& cs)
                {
                    return HYP_FORMAT("-----\n\tProperties: {}\n\tVertex Attributes: {}\n-----",
                        cs.properties.GetDebugString(), cs.vertexAttributes.ToString());
                }));

        HYP_BREAKPOINT;

        return false;
    }

    out = *it;

#ifdef HYP_SHADER_COMPILER_LOGGING
    HYP_LOG(ShaderCompiler, Debug,
        "Selected shader {} with properties: {}, attributes: {}",
        name,
        finalProperties.ToString(),
        finalProperties.GetRequiredVertexAttributes().ToString());
#endif

    Assert(out.IsValid());

    return true;
}

#pragma endregion ShaderCompiler

} // namespace Hyperion
