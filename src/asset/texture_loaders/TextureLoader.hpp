#ifndef HYPERION_V2_TEXTURE_LOADER_H
#define HYPERION_V2_TEXTURE_LOADER_H

#include <asset/Assets.hpp>
#include <scene/Node.hpp>
#include <core/Containers.hpp>

#include <Types.hpp>
#include <rendering/Texture.hpp>

namespace hyperion::v2 {

class TextureLoader : public AssetLoader
{
public:
    virtual ~TextureLoader() = default;

    virtual LoadedAsset LoadAsset(LoaderState &state) const override;

    struct TextureData
    {
        int width;
        int height;
        int num_components;
        InternalFormat format;
    };
};

} // namespace hyperion::v2

#endif