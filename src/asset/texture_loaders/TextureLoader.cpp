/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <asset/texture_loaders/TextureLoader.hpp>
#include <asset/Assets.hpp>
#include <asset/AssetRegistry.hpp>
#include <asset/TextureAsset.hpp>

#include <core/utilities/StringUtil.hpp>

#include <rendering/Texture.hpp>

#include <util/img/ImageUtil.hpp>

#include <thirdparty/stb_image.h>

#include <TextureLoader.generated.inl>

namespace hyperion {

struct LoadedTextureData
{
    int width;
    int height;
    int numComponents;
    TextureFormat format;
};

static const stbi_io_callbacks s_callbacks {
    .read = [](void* user, char* data, int size) -> int
    {
        LoaderState* state = static_cast<LoaderState*>(user);

        return int(state->stream.Read(data, SizeType(size), [](void* ptr, const unsigned char* buffer, SizeType chunkSize)
            {
                Memory::MemCpy(ptr, buffer, chunkSize);
            }));
    },
    .skip = [](void* user, int n)
    {
        LoaderState* state = static_cast<LoaderState*>(user);

        if (n < 0)
        {
            state->stream.Rewind(-n);
        }
        else
        {
            state->stream.Skip(n);
        }
    },
    .eof = [](void* user) -> int
    {
        const LoaderState* state = static_cast<LoaderState*>(user);

        return int(state->stream.Eof());
    }
};

AssetLoadResult TextureLoader::LoadAsset(LoaderState& state) const
{
    LoadedTextureData data;

    unsigned char* imageBytes = stbi_load_from_callbacks(
        &s_callbacks,
        (void*)&state,
        &data.width,
        &data.height,
        &data.numComponents,
        0);

    switch (data.numComponents)
    {
    case STBI_rgb_alpha:
        data.format = TF_RGBA8;
        break;
    case STBI_rgb:
        data.format = TF_RGB8;
        break;
    case STBI_grey_alpha:
        data.format = TF_RG8;
        break;
    case STBI_grey:
        data.format = TF_R8;
        break;
    default:
        return HYP_MAKE_ERROR(AssetLoadError, "Invalid format -- invalid number of components returned");
    }

    // data.width = 1;
    // data.height = 1;

    Name assetName = CreateNameFromDynamicString(StringUtil::StripExtension(state.filepath.Basename()));

    const SizeType imageBytesCount = SizeType(data.width)
        * SizeType(data.height)
        * SizeType(data.numComponents);

    TextureDesc textureDesc {
        TT_TEX2D,
        data.format,
        Vec3u { uint32(data.width), uint32(data.height), 1 },
        TFM_LINEAR_MIPMAP,
        TFM_LINEAR,
        TWM_REPEAT
    };

    TextureData textureData;
    textureData.imageData = ByteBuffer(imageBytesCount, imageBytes);

    const uint32 numComponents = NumComponents(data.format);

    if (numComponents == 3)
    {
        // convert to bytes per pixel = 4
        const uint32 size = textureDesc.GetByteSize();
        const uint32 faceOffsetStep = size / textureDesc.NumFaces();

        textureDesc.format = FormatChangeNumComponents(data.format, 4);

        const uint32 newSize = textureDesc.GetByteSize();
        const uint32 newFaceOffsetStep = newSize / textureDesc.NumFaces();

        ByteBuffer newByteBuffer(textureDesc.GetByteSize());

        for (uint32 i = 0; i < textureDesc.NumFaces(); i++)
        {
            ImageUtil::ConvertBPP(
                textureDesc.extent.x, textureDesc.extent.y, textureDesc.extent.z,
                numComponents, 4,
                &textureData.imageData.Data()[i * faceOffsetStep],
                &newByteBuffer.Data()[i * newFaceOffsetStep]);
        }

        HYP_LOG_TEMP("Converted texture '{}' from 3 to 4 components", assetName);

        textureData.imageData = std::move(newByteBuffer);
    }

    Handle<Texture> texture = CreateObject<Texture>(textureDesc, textureData);

    stbi_image_free(imageBytes);

    texture->SetName(assetName);

    texture->GetAsset()->Rename(assetName);
    texture->GetAsset()->SetOriginalFilepath(FilePath::Relative(state.filepath, state.assetManager->GetBasePath()));

    state.assetManager->GetAssetRegistry()->RegisterAsset("$Import/Media/Textures", texture->GetAsset());

    InitObject(texture);

    AssetLoadResult result = LoadedAsset { std::move(texture) };

    return result;
}

} // namespace hyperion
