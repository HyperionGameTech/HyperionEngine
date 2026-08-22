/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#include <RenderingPch.hpp>

#include <Rendering/Shader.hpp>

#include <Rendering/Util/ShaderPropertyDictionary.hpp>

#include <Asset/AssetRegistry.hpp>
#include <Asset/Assets.hpp>
#include <Asset/BlobStorage.hpp>

#include <Framework/EngineGlobals.hpp>

#include <Shader.generated.inl>

namespace Hyperion
{

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
    Handle<AssetRegistry> registry = GetEngineAssetRegistry();
    AssertDebug(registry.IsValid());

    if (!registry.IsValid())
    {
        return;
    }

    for (uint32 i = 0; i < uint32(shaderBlobs.Size()); i++)
    {
        BlobDataReference& ref = shaderBlobs[i];
        const ShaderModuleType moduleType = moduleTypes[i];

        if (ref.raw == nullptr && ref.key && ref.size != 0)
        {
            if (PageBlobDataFromStorage(ref))
            {
                continue;
            }

            const char* moduleTypeString = GetShaderHeaderPrefix(moduleType);

            const Name blobKey = ref.key;
            const uint64 expectedSize = ref.size;

            FileByteReader stream { registry->GetRootPath() / AssetBuckets::Shaders.GetName() / (String(*GetName()) + "." + moduleTypeString + ".raw.blob") };
            if (!stream.Eof())
            {
                if (stream.Max() != expectedSize)
                {
                    HYP_LOG(Engine, Error, "Local blob data for shader '{}' module {} is {} bytes but the manifest expects {}. Data corruption detected.",
                            GetName(), moduleTypeString, stream.Max(), expectedSize);

                    continue;
                }

                ByteBuffer buffer = stream.Read(stream.Max());

                AllocateBlobData(ref, buffer.Data(), buffer.Size(), 1);
                ref.key = blobKey;

                continue;
            }
            else
            {
                HYP_LOG(Engine, Error, "Failed to read {}", stream.GetFilepath());
            }

            HYP_LOG(Engine, Error, "Failed to page blob data for Shader {}", GetName());

            ref.readOnly = true;
        }
    }
}

void Shader::UnpageBlobData()
{
    for (BlobDataReference& ref : shaderBlobs)
    {
        if (!ref.readOnly)
        {
            FreeBlobData(ref);
        }
        
        ref.raw = nullptr;
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
