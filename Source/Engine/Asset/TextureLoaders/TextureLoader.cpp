/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#include <AssetPch.hpp>

#include <Asset/TextureLoaders/TextureLoader.hpp>
#include <Asset/Assets.hpp>
#include <Asset/AssetRegistry.hpp>

#include <Core/Utilities/StringUtil.hpp>

#include <Rendering/Texture.hpp>

#include <Util/Img/ImageUtil.hpp>

#include <stb_image.h>

#include <TextureLoader.generated.inl>

namespace Hyperion {

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

        return int(state->stream.Read(data, size_t(size)));
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
        data.format = TextureFormat::RGBA8;
        break;
    case STBI_rgb:
        data.format = TextureFormat::RGB8;
        break;
    case STBI_grey_alpha:
        data.format = TextureFormat::RG8;
        break;
    case STBI_grey:
        data.format = TextureFormat::R8;
        break;
    default:
        return HYP_MAKE_ERROR(AssetLoadError, "Invalid format -- invalid number of components returned");
    }

    // data.width = 1;
    // data.height = 1;

    Name assetName = CreateNameFromDynamicString(StringUtil::StripExtension(state.filepath.Basename()));

    const size_t imageBytesCount = size_t(data.width)
        * size_t(data.height)
        * size_t(data.numComponents);

    TextureDesc textureDesc {
        TextureType::Texture2D,
        data.format,
        Vec3u { uint32(data.width), uint32(data.height), 1 },
        TFM_LINEAR_MIPMAP,
        TFM_LINEAR,
        TWM_REPEAT
    };

    AssertDebug(TextureUtils::NumComponents(data.format) == data.numComponents);

    ByteBuffer baseMipData = ByteBuffer(imageBytesCount, imageBytes);

    if (data.numComponents == 3)
    {
        // convert to bytes per pixel = 4
        const uint32 size = textureDesc.GetByteSize();
        const uint32 faceOffsetStep = size / textureDesc.NumArrayLayers();

        textureDesc.format = TextureUtils::FormatChangeNumComponents(data.format, 4);

        const uint32 newSize = textureDesc.GetByteSize();
        const uint32 newFaceOffsetStep = newSize / textureDesc.NumArrayLayers();

        ByteBuffer newByteBuffer(textureDesc.GetByteSize());

        for (uint32 i = 0; i < textureDesc.NumArrayLayers(); i++)
        {
            ImageUtil::ConvertBPP(
                textureDesc.extent.x, textureDesc.extent.y, textureDesc.extent.z,
                data.numComponents, 4,
                &baseMipData.Data()[i * faceOffsetStep],
                &newByteBuffer.Data()[i * newFaceOffsetStep]);
        }

        HYP_LOG(Texture, Verbose, "Converted texture '{}' from 3 to 4 components", assetName);

        baseMipData = std::move(newByteBuffer);
    }

    if (state.hint == AssetLoadHint::TextureLoader_LoadAsSRGB)
    {
        textureDesc.format = TextureUtils::ChangeFormatSRGB(textureDesc.format, /* useSRGB */ true);
    }

    Texture::GenerateMipmaps(textureDesc, baseMipData);

    Handle<Texture> texture = MakeHandle<Texture>(textureDesc, baseMipData.ToByteView());

    stbi_image_free(imageBytes);

    texture->SetName(assetName);
    //texture->SetOriginalFilepath(FilePath::Relative(state.filepath, state.assetManager->GetBasePath()));

    GetCurrentAssetRegistry()->PutAssetUnique(texture);

    CheckResult(texture->Create());

    AssetLoadResult result = LoadedAsset { std::move(texture) };

    return result;
}

} // namespace Hyperion
