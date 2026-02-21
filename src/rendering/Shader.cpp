/* Copyright (c) 2026 No Tomorrow Games. All rights reserved. */

#include <RenderingPch.hpp>

#include <rendering/Shader.hpp>

#include <asset/AssetRegistry.hpp>
#include <asset/Assets.hpp>

#include <engine/EngineGlobals.hpp>

#include <Shader.generated.inl>

namespace Hyperion {

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
        const SizeType index = it - moduleTypes.Begin();

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
    for (uint32 i = 0; i < uint32(shaderBlobs.Size()); i++)
    {
        BlobDataReference& ref = shaderBlobs[i];
        const ShaderModuleType moduleType = moduleTypes[i];

        if (ref.raw == nullptr && ref.key && ref.size != 0)
        {
            BlobStorage& blobStorage = g_assetManager->GetAssetRegistry()->GetBlobStorage();

            if (!blobStorage.GetData(ref.key, ref.size, ref.raw))
            {
#ifdef HYP_EDITOR
                Handle<AssetPackage> package = GetPackage();
                Assert(package.IsValid());
                Assert(package->IsSaved());

                FileByteReader stream { package->GetSavedDirectory() / (String(*GetName()) + "." + GetShaderHeaderPrefix(moduleType) + ".raw.blob") };
                if (!stream.Eof())
                {
                    ByteBuffer buffer = stream.Read(stream.Max());

                    AllocateBlobData(ref, buffer.Data(), buffer.Size(), 1);

                    MarkDirty();

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

} // namespace Hyperion
