#include "TextureLoader.hpp"
#include <Engine.hpp>
#include <util/img/stb_image.h>

namespace hyperion::v2 {

using TextureLoader = LoaderObject<Texture, LoaderFormat::TEXTURE_2D>::Loader;

static const stbi_io_callbacks callbacks{
    .read = [](void *user, char *data, int size) -> int {
        auto *state = static_cast<LoaderState *>(user);

        return int(state->stream.Read(data, static_cast<size_t>(size), [](void *ptr, const unsigned char *buffer, size_t chunk_size) {
            std::memcpy(ptr, buffer, chunk_size);
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

        return int(state->stream.Eof());
    }
};

LoaderResult TextureLoader::LoadFn(LoaderState *state, Object &object)
{
    unsigned char *image_bytes = stbi_load_from_callbacks(
        &callbacks,
        (void *)state,
        &object.width,
        &object.height,
        &object.num_components,
        0
    );

    switch (object.num_components) {
    case STBI_rgb_alpha:
        object.format = Image::InternalFormat::TEXTURE_INTERNAL_FORMAT_RGBA8;
        break;
    case STBI_rgb:
        object.format = Image::InternalFormat::TEXTURE_INTERNAL_FORMAT_RGB8;
        break;
    case STBI_grey_alpha:
        object.format = Image::InternalFormat::TEXTURE_INTERNAL_FORMAT_RG8;
        break;
    case STBI_grey:
        object.format = Image::InternalFormat::TEXTURE_INTERNAL_FORMAT_R8;
        break;
    default:
        return {LoaderResult::Status::ERR, "Invalid format -- invalid number of components returned"};
    }

    const size_t image_bytes_count = object.width * object.height * object.num_components;

    object.data.resize(image_bytes_count);
    std::memcpy(&object.data[0], image_bytes, image_bytes_count);

    stbi_image_free(image_bytes);

    return {};
}

std::unique_ptr<Texture> TextureLoader::BuildFn(Engine *engine, const Object &object)
{
    return std::make_unique<Texture2D>(
        Extent2D{
            uint32_t(object.width),
            uint32_t(object.height)
        },
        object.format,
        Image::FilterMode::TEXTURE_FILTER_LINEAR_MIPMAP,
        Image::WrapMode::TEXTURE_WRAP_REPEAT,
        &object.data[0]
    );
}

} // namespace hyperion::v2
