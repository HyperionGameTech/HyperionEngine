/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <asset/texture_loaders/TextureLoader.hpp>

#include <Engine.hpp>

#include <util/StringUtil.hpp>
#include <util/img/stb_image.h>

namespace hyperion {

using TextureData = TextureLoader::TextureData;

static const stbi_io_callbacks callbacks {
    .read = [](void *user, char *data, int size) -> int
    {
        LoaderState *state = static_cast<LoaderState *>(user);

        return int(state->stream.Read(data, SizeType(size), [](void *ptr, const unsigned char *buffer, SizeType chunk_size)
        {
            Memory::MemCpy(ptr, buffer, chunk_size);
        }));
    },
    .skip = [](void *user, int n)
    {
        LoaderState *state = static_cast<LoaderState *>(user);

        if (n < 0) {
            state->stream.Rewind(-n);
        } else {
            state->stream.Skip(n);
        }
    },
    .eof = [](void *user) -> int
    {
        const LoaderState *state = static_cast<LoaderState *>(user);

        return int(state->stream.Eof());
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
        return { { LoaderResult::Status::ERR, "Invalid format -- invalid number of components returned" } };
    }

    //data.width = 1;
    //data.height = 1;


    const SizeType image_bytes_count = SizeType(data.width)
        * SizeType(data.height)
        * SizeType(data.num_components);

    UniquePtr<MemoryStreamedData> memory_streamed_data = UniquePtr<MemoryStreamedData>::Construct(ByteBuffer(image_bytes_count, image_bytes));

    Handle<Texture> texture = CreateObject<Texture>(
        TextureDesc
        {
            ImageType::TEXTURE_TYPE_2D,
            data.format,
            Extent3D { uint32(data.width), uint32(data.height), uint32(1) },
            FilterMode::TEXTURE_FILTER_LINEAR_MIPMAP,
            FilterMode::TEXTURE_FILTER_LINEAR,
            WrapMode::TEXTURE_WRAP_REPEAT
        },
        std::move(memory_streamed_data)
    );

    stbi_image_free(image_bytes);

    texture->SetName(CreateNameFromDynamicString(StringUtil::Basename(state.filepath.Data()).c_str()));

    return { { LoaderResult::Status::OK }, std::move(texture) };
}

} // namespace hyperion
