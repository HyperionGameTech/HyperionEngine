/* Copyright (c) 2016-2026 Andrew J. MacDonald. All rights reserved. */

#include <RenderingPch.hpp>

#include <rendering/Shader.hpp>

#include <rendering/util/ShaderPropertyDictionary.hpp>

#include <asset/AssetRegistry.hpp>
#include <asset/Assets.hpp>
#include <asset/BlobStorage.hpp>

#include <engine/EngineGlobals.hpp>

#include <Shader.generated.inl>

namespace Hyperion {

#if HYP_EDITOR
HYP_DECLARE_LOG_CHANNEL(Editor);
#endif

extern ShaderInputGroup& GetStaticDescriptorTableDeclaration();

Shader::~Shader()
{
    for (BlobDataReference& ref : shaderBlobs)
    {
        FreeBlobData(ref);
    }
}

void Shader::AddShaderModule(
    ShaderModuleType moduleType,
    UTF8StringView moduleName,
    UTF8StringView entryPointName,
    ConstByteView blobData)
{
    MarkDirty();

    auto it = moduleTypes.Find(moduleType);

    // if we already have this module type, replace the blob and entry point name
    if (it != moduleTypes.End())
    {
        const size_t index = it - moduleTypes.Begin();

        Assert(index < shaderBlobs.Size() && index < entryPointNames.Size());

        moduleNames[index] = moduleName;
        entryPointNames[index] = entryPointName;

        FreeBlobData(shaderBlobs[index]);
        AllocateBlobData(shaderBlobs[index], blobData.Data(), blobData.Size(), 1);

        return;
    }

    Assert(moduleTypes.Size() == shaderBlobs.Size()
        && moduleTypes.Size() == moduleNames.Size()
        && moduleTypes.Size() == entryPointNames.Size());

    moduleTypes.PushBack(moduleType);
    moduleNames.PushBack(moduleName);
    entryPointNames.PushBack(entryPointName);

    BlobDataReference& ref = shaderBlobs.EmplaceBack();
    AllocateBlobData(ref, blobData.Data(), blobData.Size(), 1);
}

int Shader::GetRevisionNumber() const
{
    return int(GetStaticDescriptorTableDeclaration().GetHashCode().Value() % INT32_MAX);
}

void Shader::PageBlobData()
{
    bool needSaveBlobData = false;

    for (uint32 i = 0; i < uint32(shaderBlobs.Size()); i++)
    {
        BlobDataReference& ref = shaderBlobs[i];
        const ShaderModuleType moduleType = moduleTypes[i];

        if (ref.raw == nullptr && ref.key && ref.size != 0)
        {
            BlobStorage& blobStorage = g_assetManager->GetAssetRegistry()->GetBlobStorage();

            if (!blobStorage.GetData(ref.key, ref.size, ref.raw))
            {
#if HYP_EDITOR
                Handle<AssetPackage> package = GetPackage();
                Assert(package.IsValid());
                Assert(package->IsSaved());

                const char* moduleTypeString = GetShaderHeaderPrefix(moduleType);

                FileByteReader stream { package->GetSavedDirectory() / (String(*GetName()) + "." + moduleTypeString + ".raw.blob") };
                if (!stream.Eof())
                {
                    ByteBuffer buffer = stream.Read(stream.Max());

                    AllocateBlobData(ref, buffer.Data(), buffer.Size(), 1);

                    needSaveBlobData = true;

                    continue;
                }
#endif
                HYP_FAIL("Failed to page blob data for shader {}", GetName());
            }
            else
            {
                ref.readOnly = true;
            }
        }
    }

#if HYP_EDITOR
    if (needSaveBlobData)
    {    
        BlobStorage& blobStorage = g_assetManager->GetAssetRegistry()->GetBlobStorage();

        Result saveBlobDataResult = SaveBlobData(blobStorage);

        if (saveBlobDataResult.HasError())
        {
            HYP_LOG(Editor, Error, "Failed to save local blob data: {}", saveBlobDataResult.GetError().GetMessage());
        }
    }
#endif
}

void Shader::UnpageBlobData()
{
    for (BlobDataReference& ref : shaderBlobs)
    {
        if (ref.readOnly)
        {
            ref.raw = nullptr;
        }
    }
}

Array<ShaderProperty> Shader::SerializeProperties() const
{
    Array<ShaderProperty> result;
    for (ShaderPropertyId propertyId : properties.ToArray())
    {
        ShaderProperty& property = result.EmplaceBack();
        if (!GetShaderPropertyById(propertyId, property))
        {
            HYP_LOG(Shader, Error, "Failed to find ShaderProperty with ID {} in reverse lookup map!", uint32(propertyId));

            continue;
        }
    }

    return result;
}

void Shader::DeserializeProperties(const Array<ShaderProperty>& properties)
{
    this->properties = {};

    for (const ShaderProperty& property : properties)
    {
        this->properties.Add(InternShaderProperty(property));
    }
}

} // namespace Hyperion
