#ifndef HYPERION_V2_FBX_MODEL_LOADER_HPP
#define HYPERION_V2_FBX_MODEL_LOADER_HPP

#include <asset/Assets.hpp>
#include <scene/Node.hpp>
#include <core/Containers.hpp>

#include <Types.hpp>

namespace hyperion::v2 {

class FBXModelLoader : public AssetLoader
{
public:
    virtual ~FBXModelLoader() = default;

    virtual LoadedAsset LoadAsset(LoaderState &state) const override;
};

} // namespace hyperion::v2

#endif