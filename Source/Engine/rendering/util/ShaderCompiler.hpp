/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <Core/name/Name.hpp>
#include <Core/Defines.hpp>

#include <Core/threading/Mutex.hpp>

#include <Core/containers/Set.hpp>
#include <Core/containers/Array.hpp>
#include <Core/containers/String.hpp>

#include <Core/utilities/Variant.hpp>
#include <Core/utilities/StringUtil.hpp>

#include <asset/AssetObject.hpp>

#include <rendering/Shared.hpp>

#include <util/ini/INIFile.hpp>

#include <Core/HashCode.hpp>
#include <Core/Types.hpp>

namespace Hyperion {

struct ShaderInputGroup;
class Shader;

static constexpr const char* DefaultEntryPointNames[NumShaderModuleTypes] = {
    "",                     // ShaderModuleType::None

    "VSMain",               // ShaderModuleType::Vertex
    "PSMain",               // ShaderModuleType::Pixel
    "GSMain",               // ShaderModuleType::Geometry
    "CSMain",               // ShaderModuleType::Compute

    "TaskMain",             // ShaderModuleType::Task
    "MeshMain",             // ShaderModuleType::Mesh

    "TessControlMain",      // ShaderModuleType::TessControl
    "TessEvalMain",         // ShaderModuleType::TessEval

    "RayGenMain",           // ShaderModuleType::RayGen
    "IntersectMain",        // ShaderModuleType::Intersect
    "AnyHitMain",           // ShaderModuleType::AnyHit
    "ClosestHitMain",       // ShaderModuleType::ClosestHit
    "MissMain"              // ShaderModuleType::Miss
};

static constexpr const char* ShaderModuleTypeNames[NumShaderModuleTypes] = {
    "",                     // ShaderModuleType::None

    "VERTEX_SHADER",        // ShaderModuleType::Vertex
    "PIXEL_SHADER",         // ShaderModuleType::Pixel
    "GEOMETRY_SHADER",      // ShaderModuleType::Geometry
    "COMPUTE_SHADER",       // ShaderModuleType::Compute

    "TASK_SHADER",          // ShaderModuleType::Task
    "MESH_SHADER",          // ShaderModuleType::Mesh

    "TESS_CONTROL_SHADER",  // ShaderModuleType::TessControl
    "TESS_EVAL_SHADER",     // ShaderModuleType::TessEval

    "RAY_GEN_SHADER",       // ShaderModuleType::RayGen
    "INTERSECT_SHADER",     // ShaderModuleType::Intersect
    "ANY_HIT_SHADER",       // ShaderModuleType::AnyHit
    "CLOSEST_HIT_SHADER",   // ShaderModuleType::ClosestHit
    "MISS_SHADER"           // ShaderModuleType::Miss
};

HYP_ENUM()
enum class ShaderLanguage : uint32
{
    GLSL,
    HLSL
};

HYP_ENUM()
enum class ShaderCompileTargetPlatform : uint32
{
    None        = 0x00000000,
    Windows     = 0x00000001,   // Bit 0
    Mac         = 0x00000002,   // Bit 1
    Linux       = 0x00000004,   // Bit 2
    Android     = 0x00000008,   // Bit 3
    iOS         = 0x00000010,   // Bit 4
    AllPlatforms = Windows | Mac | Linux | Android | iOS
};

HYP_MAKE_ENUM_FLAGS(ShaderCompileTargetPlatform);

HYP_ENUM()
enum class ShaderCompileTargetBackend : uint32
{
    None        = 0x00000000,
    Vulkan      = 0x00000100,   // Bit 8
    DX12        = 0x00000200,   // Bit 9
    AllBackends = Vulkan | DX12
};

HYP_MAKE_ENUM_FLAGS(ShaderCompileTargetBackend);

/*! \brief Combined bit mask type for shader compile targets (platforms + backends). */
using ShaderCompileTargetMask = uint32;

struct ShaderCompileParams
{
    EnumFlags<ShaderCompileTargetPlatform> targetPlatforms = ShaderCompileTargetPlatform::AllPlatforms;
    EnumFlags<ShaderCompileTargetBackend> targetBackends = ShaderCompileTargetBackend::AllBackends;

    ShaderCompileParams() = default;

    explicit ShaderCompileParams(EnumFlags<ShaderCompileTargetPlatform> platforms)
        : targetPlatforms(platforms)
    {
    }

    explicit ShaderCompileParams(EnumFlags<ShaderCompileTargetBackend> backends)
        : targetBackends(backends)
    {
    }

    ShaderCompileParams(EnumFlags<ShaderCompileTargetPlatform> platforms, EnumFlags<ShaderCompileTargetBackend> backends)
        : targetPlatforms(platforms),
          targetBackends(backends)
    {
    }

    /*! \brief Returns the combined bit mask of platforms and backends. */
    HYP_FORCE_INLINE ShaderCompileTargetMask GetTargetMask() const
    {
        return uint32(targetPlatforms) | uint32(targetBackends);
    }

    /*! \brief Returns true if the given platform is enabled. */
    HYP_FORCE_INLINE bool HasPlatform(ShaderCompileTargetPlatform platform) const
    {
        return targetPlatforms[platform];
    }

    /*! \brief Returns true if the given backend is enabled. */
    HYP_FORCE_INLINE bool HasBackend(ShaderCompileTargetBackend backend) const
    {
        return targetBackends[backend];
    }

    /*! \brief Returns true if Vulkan backend is enabled. */
    HYP_FORCE_INLINE bool ShouldCompileVulkan() const
    {
        return targetBackends[ShaderCompileTargetBackend::Vulkan];
    }

    /*! \brief Returns true if DX12 backend is enabled. */
    HYP_FORCE_INLINE bool ShouldCompileDX12() const
    {
        return targetBackends[ShaderCompileTargetBackend::DX12];
    }

    /*! \brief Set the target platforms mask. */
    HYP_FORCE_INLINE void SetTargetPlatforms(EnumFlags<ShaderCompileTargetPlatform> platforms)
    {
        targetPlatforms = platforms;
    }

    /*! \brief Set the target backends mask. */
    HYP_FORCE_INLINE void SetTargetBackends(EnumFlags<ShaderCompileTargetBackend> backends)
    {
        targetBackends = backends;
    }

    HYP_FORCE_INLINE HashCode GetHashCode() const
    {
        HashCode hc;
        hc.Add(uint32(targetPlatforms));
        hc.Add(uint32(targetBackends));
        return hc;
    }
};

HYP_ENUM()
enum class ProcessShaderSourcePhase : uint32
{
    BEFORE_PREPROCESS, // Raw source file before any preprocessing
    AFTER_PREPROCESS   // After preprocessor is ran the first time and inactive code behind #if/#ifdefs is stripped out.
};

HYP_ENUM()
enum class ShaderRegister : uint8
{
    NONE = 0,
    SRV,
    UAV,
    BUFFER,
    SAMPLER,
    MAX
};

static constexpr uint8 NumDescriptorSlots = uint8(ShaderRegister::MAX);

HYP_ENUM()
enum class ShaderInputSetFlags : uint8
{
    None = 0x0,
    Reference = 0x1, // is this a reference to a global (shared) set?
    Template = 0x2   // is this set intended to be used as a template for other sets? (e.g material textures)
};

HYP_MAKE_ENUM_FLAGS(ShaderInputSetFlags)

HYP_STRUCT()
struct ShaderStruct
{
    HYP_STRUCT_BODY(ShaderStruct);

    HYP_FIELD(Property = "Name", Serialize = true)
    Name name;

    HYP_FIELD(Property = "Size", Serialize = true)
    uint32 size = ~0u;

    HYP_FIELD(Property = "FieldNames", Serialize = true)
    Array<Name> fieldNames;

    HYP_FIELD(Property = "FieldTypes", Serialize = true)
    Array<ShaderStruct, DynamicAllocator> fieldTypes;

    ShaderStruct() = default;

    ShaderStruct(Name name, uint32 size = ~0u)
        : name(name),
          size(size)
    {
    }

    ShaderStruct(const ShaderStruct& other) = default;
    ShaderStruct& operator=(const ShaderStruct& other) = default;
    ShaderStruct(ShaderStruct&& other) noexcept = default;
    ShaderStruct& operator=(ShaderStruct&& other) noexcept = default;

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

    HYP_FORCE_INLINE Pair<Name, ShaderStruct&> AddField(Name fieldName, const ShaderStruct& type)
    {
        return Pair<Name, ShaderStruct&> { fieldNames.PushBack(fieldName), fieldTypes.PushBack(type) };
    }

    HYP_FORCE_INLINE Pair<Name, ShaderStruct&> GetField(size_t index)
    {
        return { fieldNames[index], fieldTypes[index] };
    }

    HYP_FORCE_INLINE const Pair<Name, const ShaderStruct&> GetField(size_t index) const
    {
        return { fieldNames[index], fieldTypes[index] };
    }

    HYP_FORCE_INLINE Optional<Pair<Name, ShaderStruct&>> FindField(StringHash fieldName)
    {
        for (size_t i = 0; i < fieldNames.Size(); i++)
        {
            if (fieldNames[i] == fieldName)
            {
                return Pair<Name, ShaderStruct&> { fieldNames[i], fieldTypes[i] };
            }
        }

        return {};
    }

    HYP_FORCE_INLINE Optional<Pair<Name, const ShaderStruct&>> FindField(StringHash fieldName) const
    {
        for (size_t i = 0; i < fieldNames.Size(); i++)
        {
            if (fieldNames[i] == fieldName)
            {
                return Pair<Name, const ShaderStruct&> { fieldNames[i], fieldTypes[i] };
            }
        }

        return {};
    }

    HYP_FORCE_INLINE bool operator<(const ShaderStruct& other) const
    {
        if (size != other.size)
        {
            return size < other.size;
        }

        if (fieldTypes.Size() != other.fieldTypes.Size())
        {
            return fieldTypes.Size() < other.fieldTypes.Size();
        }

        for (size_t i = 0; i < fieldTypes.Size(); i++)
        {
            if (fieldTypes[i] != other.fieldTypes[i])
            {
                return fieldTypes[i] < other.fieldTypes[i];
            }
        }

        return false;
    }

    HYP_FORCE_INLINE bool operator==(const ShaderStruct& other) const
    {
        return name == other.name
            && size == other.size
            && fieldNames == other.fieldNames
            && fieldTypes == other.fieldTypes;
    }

    HYP_FORCE_INLINE bool operator!=(const ShaderStruct& other) const
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

HYP_STRUCT()
struct ShaderInput
{
    HYP_STRUCT_BODY(ShaderInput);

    using ConditionFunction = bool (*)();

    HYP_FIELD(Property = "Register", Serialize = true)
    ShaderRegister slot = ShaderRegister::NONE;

    HYP_FIELD(Property = "ElementType", Serialize = true)
    ShaderInputType type = ShaderInputType::Unset;

    HYP_FIELD(Property = "Name", Serialize = true)
    Name name;

    HYP_FIELD(Property = "Count", Serialize = true)
    uint32 count = 1;

    HYP_FIELD(Property = "Size", Serialize = true)
    uint32 size = uint32(-1);

    HYP_FIELD(Property = "IsDynamic", Serialize = true)
    bool isDynamic = false;

    HYP_FIELD(Property = "BufferType", Serialize = true)
    GpuBufferType bufferType = GpuBufferType::NONE;

    HYP_FIELD(Property = "StructInfo", Serialize = true)
    ShaderStruct structInfo;

    HYP_FIELD(Property = "Category", Serialize = true)
    ShaderResourceCategory category = ShaderResourceCategory::Unknown;

    HYP_FIELD(Property = "Index", Transient = true, Serialize = false)
    uint32 index = ~0u;

    ConditionFunction cond = nullptr;

    HYP_FORCE_INLINE HashCode GetHashCode() const
    {
        HashCode hc;

        hc.Add(slot);
        hc.Add(type);
        hc.Add(name);
        hc.Add(count);
        hc.Add(size);
        hc.Add(isDynamic);
        hc.Add(bufferType);
        hc.Add(structInfo);
        hc.Add(index);

        // cond excluded intentionally

        return hc;
    }
};

HYP_STRUCT()
struct ShaderInputSet
{
    HYP_STRUCT_BODY(ShaderInputSet);

    HYP_FIELD(Property = "SetIndex", Serialize = true)
    uint32 setIndex = ~0u;

    HYP_FIELD(Property = "Name", Serialize = true)
    Name name = Name::Invalid();

    HYP_FIELD(Property = "Slots", Serialize = true)
    FixedArray<Array<ShaderInput, DynamicAllocator>, NumDescriptorSlots> slots = {};

    HYP_FIELD(Property = "Flags", Serialize = true)
    EnumFlags<ShaderInputSetFlags> flags = ShaderInputSetFlags::None;

    ShaderInputSet() = default;

    ShaderInputSet(uint32 setIndex, Name name)
        : setIndex(setIndex),
          name(name)
    {
    }

    ShaderInputSet(const ShaderInputSet& other) = default;
    ShaderInputSet& operator=(const ShaderInputSet& other) = default;

    ShaderInputSet(ShaderInputSet&& other) noexcept = default;
    ShaderInputSet& operator=(ShaderInputSet&& other) noexcept = default;

    ~ShaderInputSet() = default;

    void Add(ShaderInput shaderInput)
    {
        AssertDebug(shaderInput.slot != ShaderRegister::NONE && uint8(shaderInput.slot) < NumDescriptorSlots);

        shaderInput.index = uint32(slots[uint8(shaderInput.slot) - 1].Size());
        slots[uint8(shaderInput.slot) - 1].PushBack(std::move(shaderInput));
    }

    /*! \brief Calculate a flat index for a Descriptor that is part of this set.
        Returns -1 if not found */
    uint32 CalculateFlatIndex(ShaderRegister slot, StringHash name) const;

    ShaderInput* FindDescriptorDeclaration(StringHash name) const;

    HYP_FORCE_INLINE HashCode GetHashCode() const
    {
        HashCode hc;

        hc.Add(setIndex);
        hc.Add(name);
        hc.Add(flags);

        for (const auto& slot : slots)
        {
            for (const ShaderInput& shaderInput : slot)
            {
                hc.Add(shaderInput.GetHashCode());
            }
        }

        return hc;
    }
};

HYP_STRUCT()
struct ShaderInputGroup
{
    HYP_STRUCT_BODY(ShaderInputGroup);

    HYP_FIELD(Property = "Elements", Serialize = true)
    Array<ShaderInputSet> elements;

    ShaderInputSet* FindDescriptorSetDeclaration(StringHash name) const;
    ShaderInputSet* AddDescriptorSetDeclaration(ShaderInputSet&& inputSet);

    /*! \brief Get the index of a descriptor set in the table
        \param name The name of the descriptor set
        \return The index of the descriptor set in the table, or -1 if not found */
    HYP_FORCE_INLINE uint32 GetDescriptorSetIndex(StringHash name) const
    {
        for (const auto& it : elements)
        {
            if (it.name == name)
            {
                return it.setIndex;
            }
        }

        return ~0u;
    }

    HYP_FORCE_INLINE HashCode GetHashCode() const
    {
        HashCode hc;

        for (const ShaderInputSet& decl : elements)
        {
            hc.Add(decl.GetHashCode());
        }

        return hc;
    }

    struct DeclareSet
    {
        DeclareSet(ShaderInputGroup* table, uint32 setIndex, Name name, bool isTemplate = false)
        {
            AssertDebug(table != nullptr);

            if (table->elements.Size() <= setIndex)
            {
                table->elements.Resize(setIndex + 1);
            }

            ShaderInputSet& decl = table->elements[setIndex];
            decl.setIndex = setIndex;
            decl.name = name;

            if (isTemplate)
            {
                decl.flags |= ShaderInputSetFlags::Template;
            }
        }
    };

    struct DeclareDescriptor
    {
        DeclareDescriptor(ShaderInputGroup* table, Name setName, ShaderInputType type, ShaderRegister slotType, Name descriptorName, ShaderInput::ConditionFunction cond = nullptr, uint32 count = 1, uint32 size = ~0u, bool isDynamic = false, ShaderResourceCategory category = ShaderResourceCategory::Unknown)
        {
            AssertDebug(table != nullptr);

            uint32 setIndex = ~0u;

            for (size_t i = 0; i < table->elements.Size(); ++i)
            {
                if (table->elements[i].name == setName)
                {
                    setIndex = uint32(i);
                    break;
                }
            }

            AssertDebug(setIndex != ~0u, "Descriptor set {} not found", setName);

            ShaderInputSet& inputSet = table->elements[setIndex];
            AssertDebug(inputSet.setIndex == setIndex);

            const uint32 slotTypeIndex = uint8(slotType) - 1;
            AssertDebug(slotTypeIndex < inputSet.slots.Size());

            const uint32 slotIndex = uint32(inputSet.slots[slotTypeIndex].Size());

            ShaderInput& shaderInput = inputSet.slots[slotTypeIndex].EmplaceBack();
            shaderInput.index = slotIndex;
            shaderInput.type = type;
            shaderInput.slot = slotType;
            shaderInput.name = descriptorName;
            shaderInput.cond = cond;
            shaderInput.size = size;
            shaderInput.count = count;
            shaderInput.isDynamic = isDynamic;

            if (category != ShaderResourceCategory::Unknown)
            {
                shaderInput.category = category;
            }
            else if (type == ShaderInputType::CBV || type == ShaderInputType::CBV_Dynamic)
            {
                shaderInput.category = ShaderResourceCategory::Buffer;
            }
            else if (type == ShaderInputType::Sampler)
            {
                shaderInput.category = ShaderResourceCategory::Sampler;
            }
        }
    };
};

HYP_CLASS(AssetBucket = "ShaderBundles")
class HYP_API ShaderBundle : public AssetObject
{
    HYP_OBJECT_BODY(ShaderBundle);

public:
    HYP_FIELD(Property = "CompiledShaders")
    Array<Handle<Shader>> compiledShaders;

    HYP_FIELD(Property = "StaticProperties")
    Array<Pair<Name, ShaderProperty::Value>> staticProperties;

    HYP_FIELD(Transient)
    Array<String> errorMessages;

    ShaderBundle() = default;

    explicit ShaderBundle(Name name)
        : AssetObject(name)
    {
    }

    ~ShaderBundle() override = default;

    HYP_FORCE_INLINE bool HasErrors() const
    {
        return errorMessages.Any();
    }

    HashCode GetHashCode() const;
};

void MergeGlobalShaderProperties(ShaderPropertySet& out);

class HYP_API ShaderCompiler
{
    struct ProcessError
    {
        String errorMessage;
    };

    struct ProcessResult
    {
        String processedSource;
        Array<ProcessError> errors;
        Array<VertexAttributeDefinition> requiredAttributes;
        Array<VertexAttributeDefinition> optionalAttributes;
        Array<struct DescriptorUsage, DynamicAllocator> descriptorUsages;
        ShaderVariantPerms scannedProperties; // properties collected from shader source

        ProcessResult() = default;
        ProcessResult(const ProcessResult& other) = default;
        ProcessResult& operator=(const ProcessResult& other) = default;
        ProcessResult(ProcessResult&& other) noexcept = default;
        ProcessResult& operator=(ProcessResult&& other) noexcept = default;
        ~ProcessResult() = default;
    };

    struct ShaderRequest
    {
        ShaderPropertySet properties;
        VertexInputLayoutDesc inputLayout;
    };

public:
    ShaderCompiler();
    ShaderCompiler(const ShaderCompiler& other) = delete;
    ShaderCompiler& operator=(const ShaderCompiler& other) = delete;
    ~ShaderCompiler();

    bool CanCompileShaders() const;
    bool CanCompileShaders(const ShaderCompileParams& params) const;
    bool LoadShaderDefinitions(bool precompileShaders = false, const ShaderCompileParams& params = ShaderCompileParams());

    bool RequestShader(
        Name name,
        const ShaderPropertySet& properties,
        const VertexInputLayoutDesc& inputLayout,
        Shader*& outShader);

    bool IsGraphicsShaderBundle(Name name) const;

#if HYP_ENABLE_SHADER_RELOAD
    bool IsShaderBundleOutdated(Name name, const Time& lastCompiledTimestamp) const;
#endif

    /*! \brief Get the current shader compilation parameters. */
    HYP_FORCE_INLINE const ShaderCompileParams& GetCompileParams() const
    {
        return m_compileParams;
    }

    /*! \brief Set the shader compilation parameters. */
    HYP_FORCE_INLINE void SetCompileParams(const ShaderCompileParams& params)
    {
        m_compileParams = params;
    }

private:
    ProcessResult ProcessShaderSource(
        ProcessShaderSourcePhase phase,
        ShaderModuleType type,
        ShaderLanguage language,
        const String& source,
        const String& filename,
        const ShaderVariantPerms& perm);

    void ParseDefinitionSection(
        const INIFile::Section& section,
        ShaderBundleDecl& outDecl);

    bool HandleBundle(
        ShaderBundleDecl& decl,
        Optional<ShaderRequest> shaderRequest,
        const Time& lastSavedTimestamp,
        Handle<ShaderBundle>& inOutBundle);

    bool CompileBundle(
        const ShaderBundleDecl& decl,
        Optional<ShaderRequest> shaderRequest,
        Handle<ShaderBundle>& outBundle);

    bool LoadBundle(
        Name name,
        Optional<ShaderRequest> shaderRequest,
        Handle<ShaderBundle>& outBundle);

    INIFile* m_definitions;
    Array<ShaderBundleDecl> m_shaderBundleDecls;
    bool m_isPrecompilingShaders;
    ShaderCompileParams m_compileParams;
};

} // namespace Hyperion
