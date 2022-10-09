#include "TextureLoader.hpp"
#include <Engine.hpp>
#include <util/StringUtil.hpp>
#include <util/img/stb_image.h>

namespace hyperion::v2 {

using TextureData = TextureLoader::TextureData;

static const stbi_io_callbacks callbacks {
    .read = [](void *user, char *data, int size) -> int {
        auto *state = static_cast<LoaderState *>(user);

        return static_cast<int>(state->stream.Read(data, static_cast<size_t>(size), [](void *ptr, const unsigned char *buffer, size_t chunk_size) {
            Memory::Copy(ptr, buffer, chunk_size);
        }));
    },
    .skip = [](void *user, int n) {
        auto *state = static_cast<LoaderState *>(user);

        if (n < 0) {
            state->stream.Rewind(-n);
        } else {
            state->stream.Skip(n);
        }
    },
    .eof = [](void *user) -> int {
        const auto *state = static_cast<LoaderState *>(user);

        return static_cast<int>(state->stream.Eof());
    }
};

LoadedAsset TextureLoader::LoadAsset(LoaderState &state) const
{
    TextureData data;

    unsigned char *image_bytes = stbi_load_from_callbacks(
        &callbacks,
        (void *)&state,
        &data.width,
        &data.height,
        &data.num_components,
        0
    );

    switch (data.num_components) {
    case STBI_rgb_alpha:
        data.format = Image::InternalFormat::TEXTURE_INTERNAL_FORMAT_RGBA8;
        break;
    case STBI_rgb:
        data.format = Image::InternalFormat::TEXTURE_INTERNAL_FORMAT_RGB8;
        break;
    case STBI_grey_alpha:
        data.format = Image::InternalFormat::TEXTURE_INTERNAL_FORMAT_RG8;
        break;
    case STBI_grey:
        data.format = Image::InternalFormat::TEXTURE_INTERNAL_FORMAT_R8;
        break;
    default:
        return { { LoaderResult::Status::ERR, "Invalid format -- invalid number of components returned" }, UniquePtr<void>() };
    }

    const SizeType image_bytes_count = static_cast<SizeType>(data.width)
        * static_cast<SizeType>(data.height)
        * static_cast<SizeType>(data.num_components);

    data.data.resize(image_bytes_count);
    Memory::Copy(&data.data[0], image_bytes, image_bytes_count);

    stbi_image_free(image_bytes);

    UniquePtr<Texture> texture(new Texture2D(
        Extent2D {
            static_cast<UInt>(data.width),
            static_cast<UInt>(data.height)
        },
        data.format,
        Image::FilterMode::TEXTURE_FILTER_LINEAR_MIPMAP,
        Image::WrapMode::TEXTURE_WRAP_REPEAT,
        &data.data[0]
    ));

    texture->SetName(String(StringUtil::Basename(state.filepath).c_str()));

    return { { LoaderResult::Status::OK }, texture.Cast<void>() };
}

} // namespace hyperion::v2
