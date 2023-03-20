#ifndef HYPERION_V2_FONT_LOADER_H
#define HYPERION_V2_FONT_LOADER_H

#include <asset/Assets.hpp>
#include <scene/Node.hpp>
#include <core/Containers.hpp>
#include <font/Face.hpp>

#include <Types.hpp>

namespace hyperion::v2 {

class FontLoader : public AssetLoader
{
public:
    virtual ~FontLoader() = default;

    virtual LoadedAsset LoadAsset(LoaderState &state) const override;

    Face font_face;
};

} // namespace hyperion::v2

#endif