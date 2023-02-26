#include "TextureLoader.hpp"
#include <Engine.hpp>
#include <util/StringUtil.hpp>
#include <util/img/stb_image.h>

namespace hyperion::v2 {

using TextureData = TextureLoader::TextureData;

static const stbi_io_callbacks callbacks {
    .read = [](void *user, char *data, Int size) -> Int {
        LoaderState *state = static_cast<LoaderState *>(user);

        return Int(state->stream.Read(data, SizeType(size), [](void *ptr, const unsigned char *buffer, SizeType chunk_size) {
            Memory::MemCpy(ptr, buffer, chunk_size);
        }));
    },
    .skip = [](void *user, Int n) {
        LoaderState *state = static_cast<LoaderState *>(user);

        if (n < 0) {
            state->stream.Rewind(-n);
        } else {
            state->stream.Skip(n);
        }
    },
    .eof = [](void *user) -> Int {
        const LoaderState *state = static_cast<LoaderState *>(user);

        return Int(state->stream.Eof());
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
        data.format = InternalFormat::RGBA8;
        break;
    case STBI_rgb:
        data.format = InternalFormat::RGB8;
        break;
    case STBI_grey_alpha:
        data.format = InternalFormat::RG8;
        break;
    case STBI_grey:
        data.format = InternalFormat::R8;
        break;
    default:
        return { { LoaderResult::Status::ERR, "Invalid format -- invalid number of components returned" }, UniquePtr<void>() };
    }

    const SizeType image_bytes_count = SizeType(data.width)
        * SizeType(data.height)
        * SizeType(data.num_components);

    data.data.resize(image_bytes_count);
    Memory::MemCpy(&data.data[0], image_bytes, image_bytes_count);

    stbi_image_free(image_bytes);

    UniquePtr<Handle<Texture>> texture(new Handle<Texture>(CreateObject<Texture>(Texture2D(
        Extent2D {
            UInt(data.width),
            UInt(data.height)
        },
        data.format,
        FilterMode::TEXTURE_FILTER_LINEAR_MIPMAP,
        WrapMode::TEXTURE_WRAP_REPEAT,
        &data.data[0]
    ))));

    (*texture)->SetName(CreateNameFromDynamicString(StringUtil::Basename(state.filepath).c_str()));

    return { { LoaderResult::Status::OK }, texture.Cast<void>() };
}

} // namespace hyperion::v2
