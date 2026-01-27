/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <core/Name.hpp>
#include <core/Defines.hpp>

#include <core/threading/Mutex.hpp>

#include <core/containers/HashSet.hpp>
#include <core/containers/Array.hpp>
#include <core/containers/String.hpp>

#include <core/utilities/Variant.hpp>
#include <core/utilities/StringUtil.hpp>

#include <rendering/Shared.hpp>

#include <util/ini/INIFile.hpp>

#include <core/HashCode.hpp>
#include <core/Types.hpp>

namespace Hyperion {

struct DescriptorTableDeclaration;

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

    "RayGen",               // ShaderModuleType::RayGen
    "Intersect",            // ShaderModuleType::Intersect
    "AnyHit",               // ShaderModuleType::AnyHit
    "ClosestHit",           // ShaderModuleType::ClosestHit
    "Miss"                  // ShaderModuleType::Miss
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
enum class ProcessShaderSourcePhase : uint32
{
    BEFORE_PREPROCESS, // Raw source file before any preprocessing
    AFTER_PREPROCESS   // After preprocessor is ran the first time and inactive code behind #if/#ifdefs is stripped out.
};

HYP_ENUM()
enum class DescriptorSlot : uint8
{
    NONE = 0,
    SRV,
    UAV,
    BUFFER,
    SAMPLER,
    MAX
};

static constexpr uint8 NumDescriptorSlots = uint8(DescriptorSlot::MAX);

HYP_ENUM()
enum class DescriptorSetDeclarationFlags : uint8
{
    NONE = 0x0,
    REFERENCE = 0x1, // is this a reference to a global descriptor set declaration?
    TEMPLATE = 0x2   // is this descriptor set intended to be used as a template for other sets? (e.g material textures)
};

HYP_MAKE_ENUM_FLAGS(DescriptorSetDeclarationFlags)

HYP_STRUCT()
struct DescriptorDeclaration
{
    HYP_STRUCT_BODY(DescriptorDeclaration);

    using ConditionFunction = bool (*)();

    HYP_FIELD(Property = "Slot", Serialize = true)
    DescriptorSlot slot = DescriptorSlot::NONE;

    HYP_FIELD(Property = "ElementType", Serialize = true)
    DescriptorType type = DescriptorType::UNSET;

    HYP_FIELD(Property = "Name", Serialize = true)
    Name name;

    HYP_FIELD(Property = "Count", Serialize = true)
    uint32 count = 1;

    HYP_FIELD(Property = "Size", Serialize = true)
    uint32 size = uint32(-1);

    HYP_FIELD(Property = "IsDynamic", Serialize = true)
    bool isDynamic = false;

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
        hc.Add(index);

        // cond excluded intentionally

        return hc;
    }
};

HYP_STRUCT()
struct DescriptorSetDeclaration
{
    HYP_STRUCT_BODY(DescriptorSetDeclaration);

    HYP_FIELD(Property = "SetIndex", Serialize = true)
    uint32 setIndex = ~0u;

    HYP_FIELD(Property = "Name", Serialize = true)
    Name name = Name::Invalid();

    HYP_FIELD(Property = "Slots", Serialize = true)
    FixedArray<Array<DescriptorDeclaration, DynamicAllocator>, NumDescriptorSlots> slots = {};

    HYP_FIELD(Property = "Flags", Serialize = true)
    EnumFlags<DescriptorSetDeclarationFlags> flags = DescriptorSetDeclarationFlags::NONE;

    DescriptorSetDeclaration() = default;

    DescriptorSetDeclaration(uint32 setIndex, Name name)
        : setIndex(setIndex),
          name(name)
    {
    }

    DescriptorSetDeclaration(const DescriptorSetDeclaration& other) = default;
    DescriptorSetDeclaration& operator=(const DescriptorSetDeclaration& other) = default;
    DescriptorSetDeclaration(DescriptorSetDeclaration&& other) noexcept = default;
    DescriptorSetDeclaration& operator=(DescriptorSetDeclaration&& other) noexcept = default;
    ~DescriptorSetDeclaration() = default;

    HYP_FORCE_INLINE void AddDescriptorDeclaration(DescriptorDeclaration decl)
    {
        AssertDebug(decl.slot != DescriptorSlot::NONE && uint8(decl.slot) < NumDescriptorSlots);

        decl.index = uint32(slots[uint8(decl.slot) - 1].Size());
        slots[uint8(decl.slot) - 1].PushBack(std::move(decl));
    }

    /*! \brief Calculate a flat index for a Descriptor that is part of this set.
        Returns -1 if not found */
    uint32 CalculateFlatIndex(DescriptorSlot slot, StringHash name) const;

    DescriptorDeclaration* FindDescriptorDeclaration(StringHash name) const;

    HYP_FORCE_INLINE HashCode GetHashCode() const
    {
        HashCode hc;

        hc.Add(setIndex);
        hc.Add(name);
        hc.Add(flags);

        for (const auto& slot : slots)
        {
            for (const auto& decl : slot)
            {
                hc.Add(decl.GetHashCode());
            }
        }

        return hc;
    }
};

HYP_STRUCT()
struct DescriptorTableDeclaration
{
    HYP_STRUCT_BODY(DescriptorTableDeclaration);

    HYP_FIELD(Property = "Elements", Serialize = true)
    Array<DescriptorSetDeclaration> elements;

    DescriptorSetDeclaration* FindDescriptorSetDeclaration(StringHash name) const;
    DescriptorSetDeclaration* AddDescriptorSetDeclaration(DescriptorSetDeclaration&& descriptorSetDeclaration);

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

        for (const DescriptorSetDeclaration& decl : elements)
        {
            hc.Add(decl.GetHashCode());
        }

        return hc;
    }

    struct DeclareSet
    {
        DeclareSet(DescriptorTableDeclaration* table, uint32 setIndex, Name name, bool isTemplate = false)
        {
            AssertDebug(table != nullptr);

            if (table->elements.Size() <= setIndex)
            {
                table->elements.Resize(setIndex + 1);
            }

            DescriptorSetDeclaration& decl = table->elements[setIndex];
            decl.setIndex = setIndex;
            decl.name = name;

            if (isTemplate)
            {
                decl.flags |= DescriptorSetDeclarationFlags::TEMPLATE;
            }
        }
    };

    struct DeclareDescriptor
    {
        DeclareDescriptor(DescriptorTableDeclaration* table, Name setName, DescriptorType type, DescriptorSlot slotType, Name descriptorName, DescriptorDeclaration::ConditionFunction cond = nullptr, uint32 count = 1, uint32 size = ~0u, bool isDynamic = false)
        {
            AssertDebug(table != nullptr);

            uint32 setIndex = ~0u;

            for (SizeType i = 0; i < table->elements.Size(); ++i)
            {
                if (table->elements[i].name == setName)
                {
                    setIndex = uint32(i);
                    break;
                }
            }

            AssertDebug(setIndex != ~0u, "Descriptor set {} not found", setName);

            DescriptorSetDeclaration& descriptorSetDecl = table->elements[setIndex];
            AssertDebug(descriptorSetDecl.setIndex == setIndex);

            const uint32 slotTypeIndex = uint8(slotType) - 1;
            AssertDebug(slotTypeIndex < descriptorSetDecl.slots.Size());

            const uint32 slotIndex = uint32(descriptorSetDecl.slots[slotTypeIndex].Size());

            DescriptorDeclaration& descriptorDecl = descriptorSetDecl.slots[slotTypeIndex].EmplaceBack();
            descriptorDecl.index = slotIndex;
            descriptorDecl.type = type;
            descriptorDecl.slot = slotType;
            descriptorDecl.name = descriptorName;
            descriptorDecl.cond = cond;
            descriptorDecl.size = size;
            descriptorDecl.count = count;
            descriptorDecl.isDynamic = isDynamic;
        }
    };
};

HYP_STRUCT()
struct HYP_API CompiledShader
{
    HYP_STRUCT_BODY(CompiledShader);

    HYP_FIELD(Property = "Name")
    Name name;

    HYP_FIELD(Property = "PropertySet")
    ShaderPropertySet properties;

    HYP_FIELD(Property = "VertexAttributes")
    VertexAttributeSet vertexAttributes;

    HYP_FIELD(Property = "DescriptorTableDeclaration")
    DescriptorTableDeclaration descriptorTableDeclaration;

    HYP_FIELD(Property = "ShaderModuleTypes")
    Array<ShaderModuleType> moduleTypes;

    HYP_FIELD(Property = "ShaderModuleNames")
    Array<String> moduleNames;

    HYP_FIELD(Property = "EntryPointNames")
    Array<String> entryPointNames;

    HYP_FIELD(Property = "ShaderBlobs", Compressed)
    Array<ByteBuffer> shaderBlobs;

    HYP_FIELD(Property = "PropertySetHashCode")
    HashCode propertySetHashCode;

    /// ===== Serialization only =====
    HYP_METHOD(Property = "RevisionNumber", NoScriptBindings)
    uint64 GetRevisionNumber() const;
    /// ==============================

    CompiledShader() = default;

    CompiledShader(const CompiledShader& other);
    CompiledShader& operator=(const CompiledShader& other);

    CompiledShader(CompiledShader&& other) noexcept;
    CompiledShader& operator=(CompiledShader&& other) noexcept;

    ~CompiledShader();

    HYP_FORCE_INLINE explicit operator bool() const
    {
        return IsValid();
    }

    HYP_FORCE_INLINE bool IsValid() const
    {
        return name.IsValid()
            && shaderBlobs.Any()
            && moduleTypes.Size() == shaderBlobs.Size()
            && moduleNames.Size() == shaderBlobs.Size()
            && entryPointNames.Size() == shaderBlobs.Size();
    }

    HYP_FORCE_INLINE const DescriptorTableDeclaration* GetDescriptorTableDeclaration() const
    {
        // \TODO return reference
        return &descriptorTableDeclaration;
    }

    void AddShaderModule(
        ShaderModuleType moduleType,
        UTF8StringView moduleName,
        UTF8StringView entryPointName,
        ByteBuffer&& shaderBlob);

    void AddShaderModule(
        ShaderModuleType moduleType,
        UTF8StringView moduleName,
        ByteBuffer&& shaderBlob)
    {
        AddShaderModule(moduleType, moduleName, DefaultEntryPointNames[uint8(moduleType)], std::move(shaderBlob));
    }

    bool GetShaderModuleInfo(
        uint32 index,
        ShaderModuleType& outModuleType,
        String& outModuleName,
        String& outEntryPointName,
        ConstByteView& outShaderBlob) const
    {
        if (!IsValid() || index < 0 || index >= moduleTypes.Size())
        {
            return false;
        }

        outModuleType = moduleTypes[index];
        outModuleName = moduleNames[index];
        outEntryPointName = entryPointNames[index];
        outShaderBlob = shaderBlobs[index].ToByteView();

        return true;
    }

    HYP_FORCE_INLINE HashCode GetHashCode() const
    {
        HashCode hc;
        hc.Add(name.GetHashCode());
        hc.Add(properties.GetHashCode());
        hc.Add(vertexAttributes.GetHashCode());
        hc.Add(moduleTypes.GetHashCode());
        hc.Add(moduleNames.GetHashCode());
        hc.Add(entryPointNames.GetHashCode());
        hc.Add(shaderBlobs.GetHashCode());
        hc.Add(propertySetHashCode);

        return hc;
    }
};

HYP_STRUCT()
struct CompiledShaderBatch
{
    HYP_STRUCT_BODY(CompiledShaderBatch);

    HYP_FIELD()
    Array<CompiledShader> compiledShaders;

    HYP_FIELD()
    Array<String> errorMessages;

    CompiledShaderBatch() = default;

    CompiledShaderBatch(const CompiledShaderBatch& other)
        : compiledShaders(other.compiledShaders),
          errorMessages(other.errorMessages)
    {
    }

    CompiledShaderBatch& operator=(const CompiledShaderBatch& other)
    {
        if (this == &other)
        {
            return *this;
        }

        compiledShaders = other.compiledShaders;
        errorMessages = other.errorMessages;

        return *this;
    }

    CompiledShaderBatch(CompiledShaderBatch&& other) noexcept
        : compiledShaders(std::move(other.compiledShaders)),
          errorMessages(std::move(other.errorMessages))
    {
    }

    CompiledShaderBatch& operator=(CompiledShaderBatch&& other) noexcept
    {
        if (this == &other)
        {
            return *this;
        }

        compiledShaders = std::move(other.compiledShaders);
        errorMessages = std::move(other.errorMessages);

        return *this;
    }

    ~CompiledShaderBatch() = default;

    HYP_FORCE_INLINE bool HasErrors() const
    {
        return errorMessages.Any();
    }

    HYP_FORCE_INLINE HashCode GetHashCode() const
    {
        return compiledShaders.GetHashCode();
    }
};

void MergeGlobalShaderProperties(ShaderVariant& out);

class ShaderCompiler
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

        ProcessResult() = default;
        ProcessResult(const ProcessResult& other) = default;
        ProcessResult& operator=(const ProcessResult& other) = default;
        ProcessResult(ProcessResult&& other) noexcept = default;
        ProcessResult& operator=(ProcessResult&& other) noexcept = default;
        ~ProcessResult() = default;
    };

public:
    ShaderCompiler();
    ShaderCompiler(const ShaderCompiler& other) = delete;
    ShaderCompiler& operator=(const ShaderCompiler& other) = delete;
    ~ShaderCompiler();

    HYP_API bool CanCompileShaders() const;
    HYP_API bool LoadShaderDefinitions(bool precompileShaders = false);

    HYP_API CompiledShader GetCompiledShader(Name name);
    HYP_API CompiledShader GetCompiledShader(Name name, const ShaderVariant& properties);

    HYP_API bool GetCompiledShader(
        Name name,
        const ShaderVariant& properties,
        CompiledShader& out);

private:
    ProcessResult ProcessShaderSource(
        ProcessShaderSourcePhase phase,
        ShaderModuleType type,
        ShaderLanguage language,
        const String& source,
        const String& filename,
        const ShaderVariant& properties);

    void ParseDefinitionSection(
        const INIFile::Section& section,
        ShaderBundleDecl& outShaderBundleDecl);

    bool CompileBundle(
        ShaderBundleDecl& shaderBundleDecl,
        CompiledShaderBatch& out)
    {
        return CompileBundle(shaderBundleDecl, ShaderVariant(), out, false);
    }

    bool CompileBundle(
        ShaderBundleDecl& shaderBundleDecl,
        const ShaderVariant& additionalProperties,
        CompiledShaderBatch& out,
        bool onlyCompileRequestedVersions = false);

    bool HandleCompiledShaderBatch(
        ShaderBundleDecl& shaderBundleDecl,
        const ShaderVariant& additionalProperties,
        const FilePath& outputFilePath,
        CompiledShaderBatch& batch);

    bool LoadOrCompileBatch(
        Name name,
        const ShaderVariant& additionalProperties,
        CompiledShaderBatch& out);

    INIFile* m_definitions;
    Array<ShaderBundleDecl> m_shaderBundleDecls;
};

} // namespace Hyperion
