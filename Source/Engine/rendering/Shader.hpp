/* Copyright (c) 2026 No Tomorrow Games. All rights reserved. */

#pragma once

#include <Core/Defines.hpp>

#include <Core/containers/Array.hpp>
#include <Core/containers/String.hpp>

#include <Core/utilities/Pair.hpp>

#include <asset/AssetObject.hpp>

#include <rendering/util/ShaderCompiler.hpp>

namespace Hyperion {

static constexpr const char* GetShaderHeaderPrefix(ShaderModuleType moduleType)
{
    switch (moduleType)
    {
    case ShaderModuleType::Vertex:
        return "VS";
    case ShaderModuleType::Pixel:
        return "PS";
    case ShaderModuleType::Geometry:
        return "GS";
    case ShaderModuleType::Compute:
        return "CS";
    case ShaderModuleType::Task:
        return "TSK";
    case ShaderModuleType::Mesh:
        return "MSH";
    case ShaderModuleType::TessControl:
        return "TCS";
    case ShaderModuleType::TessEval:
        return "TES";
    case ShaderModuleType::RayGen:
        return "RGS";
    case ShaderModuleType::Intersect:
        return "IS";
    case ShaderModuleType::AnyHit:
        return "AHS";
    case ShaderModuleType::ClosestHit:
        return "CHS";
    case ShaderModuleType::Miss:
        return "MS";
    default:
        return "";
    }
}

HYP_CLASS()
class HYP_API Shader : public AssetObject
{
    HYP_OBJECT_BODY(Shader);

public:
    HYP_FIELD(Property = "BaseName")
    Name baseName;

    HYP_FIELD(Property = "PropertySet")
    ShaderPropertySet properties;

    HYP_FIELD(Property = "VertexAttributes")
    VertexAttributeSet vertexAttributes;

    HYP_FIELD(Property = "ShaderInputGroup")
    ShaderInputGroup inputGroup;

    HYP_FIELD(Property = "ShaderModuleTypes")
    Array<ShaderModuleType> moduleTypes;

    HYP_FIELD(Property = "ShaderModuleNames")
    Array<String> moduleNames;

    HYP_FIELD(Property = "EntryPointNames")
    Array<String> entryPointNames;

    HYP_FIELD(Property = "ShaderBlobs")
    Array<BlobDataReference> shaderBlobs;

    HYP_FIELD(Property = "PropertySetHashCode")
    HashCode propertySetHashCode;

    /// ===== Serialization only =====
    HYP_METHOD(Property = "RevisionNumber", NoScriptBindings)
    int GetRevisionNumber() const;
    /// ==============================

    Shader() = default;

    explicit Shader(Name name)
        : AssetObject(name)
    {
    }

    Shader(const Shader& other) = delete;
    Shader& operator=(const Shader& other) = delete;

    Shader(Shader&& other) noexcept = delete;
    Shader& operator=(Shader&& other) noexcept = delete;

    ~Shader() override;

    HYP_FORCE_INLINE explicit operator bool() const
    {
        return IsValid();
    }

    HYP_FORCE_INLINE bool IsValid() const
    {
        return m_name.IsValid()
            && shaderBlobs.Any()
            && moduleTypes.Size() == shaderBlobs.Size()
            && moduleNames.Size() == shaderBlobs.Size()
            && entryPointNames.Size() == shaderBlobs.Size();
    }

    HYP_FORCE_INLINE const ShaderInputGroup* GetDescriptorTableDeclaration() const
    {
        // \TODO return reference
        return &inputGroup;
    }

    void AddShaderModule(
        ShaderModuleType moduleType,
        UTF8StringView moduleName,
        UTF8StringView entryPointName,
        ConstByteView blobData);

    void AddShaderModule(
        ShaderModuleType moduleType,
        UTF8StringView moduleName,
        ConstByteView blobData)
    {
        AddShaderModule(moduleType, moduleName, DefaultEntryPointNames[uint8(moduleType)], blobData);
    }

    bool GetShaderModuleInfo(
        uint32 index,
        ShaderModuleType& outModuleType,
        String& outModuleName,
        String& outEntryPointName,
        ConstByteView& outBlobData) const
    {
        if (!IsValid() || index < 0 || index >= moduleTypes.Size())
        {
            return false;
        }

        AssertDebug(shaderBlobs[index].raw != nullptr, "Shader not loaded, cannot get blob data");

        outModuleType = moduleTypes[index];
        outModuleName = moduleNames[index];
        outEntryPointName = entryPointNames[index];
        outBlobData = ConstByteView(reinterpret_cast<const ubyte*>(shaderBlobs[index].raw), shaderBlobs[index].size);

        return true;
    }

    HYP_FORCE_INLINE HashCode GetHashCode() const
    {
        HashCode hc;
        hc.Add(m_name.GetHashCode());
        hc.Add(properties.GetHashCode());
        hc.Add(vertexAttributes.GetHashCode());
        hc.Add(moduleTypes.GetHashCode());
        hc.Add(moduleNames.GetHashCode());
        hc.Add(entryPointNames.GetHashCode());
        hc.Add(shaderBlobs.GetHashCode());
        hc.Add(propertySetHashCode);

        return hc;
    }

protected:
    void PageBlobData() override;
    void UnpageBlobData() override;

    void CollectBlobDataReferences(Array<Tuple<const char*, uint16, BlobDataReference*>>& outReferences) override
    {
        for (uint32 i = 0; i < uint32(shaderBlobs.Size()); i++)
        {
            const char* magic = GetShaderHeaderPrefix(moduleTypes[i]);
            uint16 version = 1;

            outReferences.EmplaceBack(magic, version, &shaderBlobs[i]);
        }
    }
};

} // namespace Hyperion