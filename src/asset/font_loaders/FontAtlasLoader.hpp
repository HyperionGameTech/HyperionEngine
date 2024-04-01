#ifndef HYPERION_V2_FONT_ATLAS_LOADER_H
#define HYPERION_V2_FONT_ATLAS_LOADER_H

#include <asset/Assets.hpp>

#include <scene/Node.hpp>

#include <core/Containers.hpp>

#include <rendering/font/Face.hpp>

#include <Types.hpp>

namespace hyperion::v2 {

class FontAtlasLoader : public AssetLoader
{
public:
    virtual ~FontAtlasLoader() = default;

    virtual LoadedAsset LoadAsset(LoaderState &state) const override;
};

} // namespace hyperion::v2

#endif