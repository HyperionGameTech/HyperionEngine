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

#include <core/math/Vertex.hpp>

#include <rendering/Shader.hpp>

#include <util/ini/INIFile.hpp>

#include <core/HashCode.hpp>
#include <core/Types.hpp>

namespace Hyperion {

struct DescriptorTableDeclaration;

enum DescriptorSlot : uint32;

HYP_ENUM()
enum class ShaderLanguage : uint32
{
    GLSL,
    HLSL
};

HYP_ENUM()
enum class ProcessShaderSourcePhase : uint32
{
    BEFORE_PREPROCESS,
    AFTER_PREPROCESS
};

HYP_ENUM()
enum class DescriptorUsageFlags : uint32
{
    NONE = 0x0,
    DYNAMIC = 0x1
};

HYP_MAKE_ENUM_FLAGS(DescriptorUsageFlags)

HYP_STRUCT()
struct DescriptorUsageType
{
    HYP_STRUCT_BODY(DescriptorUsageType);

    HYP_FIELD(Property = "Name", Serialize = true)
    Name name;

    HYP_FIELD(Property = "Size", Serialize = true)
    uint32 size = ~0u;

    HYP_FIELD(Property = "FieldNames", Serialize = true)
    Array<Name> fieldNames;

    HYP_FIELD(Property = "FieldTypes", Serialize = true)
    Array<DescriptorUsageType, DynamicAllocator> fieldTypes;

    DescriptorUsageType() = default;

    DescriptorUsageType(Name name, uint32 size = ~0u)
        : name(name),
          size(size)
    {
    }

    DescriptorUsageType(const DescriptorUsageType& other) = default;
    DescriptorUsageType& operator=(const DescriptorUsageType& other) = default;
    DescriptorUsageType(DescriptorUsageType&& other) noexcept = default;
    DescriptorUsageType& operator=(DescriptorUsageType&& other) noexcept = default;

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

    HYP_FORCE_INLINE Pair<Name, DescriptorUsageType&> AddField(Name fieldName, const DescriptorUsageType& type)
    {
        return Pair<Name, DescriptorUsageType&> { fieldNames.PushBack(fieldName), fieldTypes.PushBack(type) };
    }

    HYP_FORCE_INLINE Pair<Name, DescriptorUsageType&> GetField(SizeType index)
    {
        return { fieldNames[index], fieldTypes[index] };
    }

    HYP_FORCE_INLINE const Pair<Name, const DescriptorUsageType&> GetField(SizeType index) const
    {
        return { fieldNames[index], fieldTypes[index] };
    }

    HYP_FORCE_INLINE Optional<Pair<Name, DescriptorUsageType&>> FindField(StringHash fieldName)
    {
        for (SizeType i = 0; i < fieldNames.Size(); i++)
        {
            if (fieldNames[i] == fieldName)
            {
                return Pair<Name, DescriptorUsageType&> { fieldNames[i], fieldTypes[i] };
            }
        }

        return {};
    }

    HYP_FORCE_INLINE Optional<Pair<Name, const DescriptorUsageType&>> FindField(StringHash fieldName) const
    {
        for (SizeType i = 0; i < fieldNames.Size(); i++)
        {
            if (fieldNames[i] == fieldName)
            {
                return Pair<Name, const DescriptorUsageType&> { fieldNames[i], fieldTypes[i] };
            }
        }

        return {};
    }

    HYP_FORCE_INLINE bool operator<(const DescriptorUsageType& other) const
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

    HYP_FORCE_INLINE bool operator==(const DescriptorUsageType& other) const
    {
        return name == other.name
            && size == other.size
            && fieldNames == other.fieldNames
            && fieldTypes == other.fieldTypes;
    }

    HYP_FORCE_INLINE bool operator!=(const DescriptorUsageType& other) const
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
struct DescriptorUsage
{
    HYP_STRUCT_BODY(DescriptorUsage);

    HYP_FIELD(Property = "Slot", Serialize = true)
    DescriptorSlot slot;

    HYP_FIELD(Property = "SetName", Serialize = true)
    Name setName;

    HYP_FIELD(Property = "DescriptorName", Serialize = true)
    Name descriptorName;

    HYP_FIELD(Property = "Type", Serialize = true)
    DescriptorUsageType type;

    HYP_FIELD(Property = "Flags", Serialize = true)
    EnumFlags<DescriptorUsageFlags> flags;

    HYP_FIELD(Property = "Params", Serialize = true)
    HashMap<String, String> params;

    DescriptorUsage()
        : slot((DescriptorSlot)0),
          setName(Name::Invalid()),
          flags(DescriptorUsageFlags::NONE)
    {
    }

    DescriptorUsage(DescriptorSlot slot, Name setName, Name descriptorName, EnumFlags<DescriptorUsageFlags> flags = DescriptorUsageFlags::NONE, HashMap<String, String> params = {})
        : slot(slot),
          setName(setName),
          descriptorName(descriptorName),
          flags(flags),
          params(std::move(params))
    {
    }

    DescriptorUsage(const DescriptorUsage& other)
        : slot(other.slot),
          setName(other.setName),
          descriptorName(other.descriptorName),
          type(other.type),
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
        setName = other.setName;
        descriptorName = other.descriptorName;
        type = other.type;
        flags = other.flags;
        params = other.params;

        return *this;
    }

    DescriptorUsage(DescriptorUsage&& other) noexcept
        : slot(other.slot),
          setName(std::move(other.setName)),
          descriptorName(std::move(other.descriptorName)),
          type(std::move(other.type)),
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
        setName = std::move(other.setName);
        descriptorName = std::move(other.descriptorName);
        type = std::move(other.type);
        flags = other.flags;
        params = std::move(other.params);

        return *this;
    }

    ~DescriptorUsage() = default;

    HYP_FORCE_INLINE bool operator==(const DescriptorUsage& other) const
    {
        return slot == other.slot
            && setName == other.setName
            && descriptorName == other.descriptorName
            && type == other.type
            && flags == other.flags
            && params == other.params;
    }

    HYP_FORCE_INLINE bool operator!=(const DescriptorUsage& other) const
    {
        return slot != other.slot
            || setName != other.setName
            || descriptorName != other.descriptorName
            || type != other.type
            || flags != other.flags
            || params != other.params;
    }

    HYP_FORCE_INLINE bool operator<(const DescriptorUsage& other) const
    {
        if (slot != other.slot)
        {
            return slot < other.slot;
        }

        if (setName != other.setName)
        {
            return setName < other.setName;
        }

        if (descriptorName != other.descriptorName)
        {
            return descriptorName < other.descriptorName;
        }

        if (type != other.type)
        {
            return type < other.type;
        }

        if (flags != other.flags)
        {
            return uint32(flags) < uint32(other.flags);
        }

        return false;
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
        if (type.HasExplicitSize())
        {
            return type.size;
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
        hc.Add(setName.GetHashCode());
        hc.Add(descriptorName.GetHashCode());
        hc.Add(type);
        hc.Add(flags);
        hc.Add(params.GetHashCode());

        return hc;
    }
};

HYP_STRUCT()
struct DescriptorUsageSet
{
    HYP_STRUCT_BODY(DescriptorUsageSet);

    HYP_FIELD()
    FlatSet<DescriptorUsage> elements;

    void BuildDescriptorTableDeclaration(DescriptorTableDeclaration& table) const;

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

HYP_STRUCT()
struct HYP_API CompiledShader
{
    HYP_STRUCT_BODY(CompiledShader);

    HYP_FIELD(Property = "Definition")
    ShaderDefinition definition;

    HYP_FIELD(Property = "DescriptorTableDeclaration", Transient) // built after load, not serialized
    DescriptorTableDeclaration* descriptorTableDeclaration = nullptr;

    HYP_FIELD(Property = "DescriptorUsageSet")
    DescriptorUsageSet descriptorUsageSet;

    HYP_FIELD(Property = "EntryPointName")
    String entryPointName = "main";

    HYP_FIELD(Property = "Modules", Compressed)
    FixedArray<ByteBuffer, SMT_MAX> modules;

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
        return definition.IsValid()
            && AnyOf(modules, &ByteBuffer::Any);
    }

    HYP_FORCE_INLINE Name GetName() const
    {
        return definition.name;
    }

    HYP_FORCE_INLINE ShaderDefinition& GetDefinition()
    {
        return definition;
    }

    HYP_FORCE_INLINE const ShaderDefinition& GetDefinition() const
    {
        return definition;
    }

    HYP_FORCE_INLINE const DescriptorTableDeclaration* GetDescriptorTableDeclaration() const
    {
        return descriptorTableDeclaration;
    }

    HYP_FORCE_INLINE const String& GetEntryPointName() const
    {
        return entryPointName;
    }

    HYP_FORCE_INLINE const ShaderProperties& GetProperties() const
    {
        return definition.properties;
    }

    HYP_FORCE_INLINE HashCode GetHashCode() const
    {
        HashCode hc;
        hc.Add(definition.GetHashCode());
        hc.Add(modules.GetHashCode());

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

void MergeGlobalShaderProperties(ShaderProperties& out);

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
        Array<DescriptorUsage> descriptorUsages;

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
    HYP_API CompiledShader GetCompiledShader(Name name, const ShaderProperties& properties);

    HYP_API bool GetCompiledShader(
        Name name,
        const ShaderProperties& properties,
        CompiledShader& out);

private:
    ProcessResult ProcessShaderSource(
        ProcessShaderSourcePhase phase,
        ShaderModuleType type,
        ShaderLanguage language,
        const String& source,
        const String& filename,
        const ShaderProperties& properties);

    void ParseDefinitionSection(
        const INIFile::Section& section,
        ShaderBundleDecl& outShaderBundleDecl);

    bool CompileBundle(
        ShaderBundleDecl& ShaderBundleDecl,
        CompiledShaderBatch& out)
    {
        return CompileBundle(ShaderBundleDecl, ShaderProperties(), out, false);
    }

    bool CompileBundle(
        ShaderBundleDecl& ShaderBundleDecl,
        const ShaderProperties& additionalVersions,
        CompiledShaderBatch& out,
        bool onlyCompileRequestedVersions = false);

    bool HandleCompiledShaderBatch(
        ShaderBundleDecl& ShaderBundleDecl,
        const ShaderProperties& additionalVersions,
        const FilePath& outputFilePath,
        CompiledShaderBatch& batch);

    bool LoadOrCompileBatch(
        Name name,
        const ShaderProperties& additionalVersions,
        CompiledShaderBatch& out);

    INIFile* m_definitions;
    Array<ShaderBundleDecl> m_shaderBundleDecls;
};

} // namespace Hyperion
