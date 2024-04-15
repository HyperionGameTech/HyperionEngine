/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_V2_FBOM_MODEL_LOADER_H
#define HYPERION_V2_FBOM_MODEL_LOADER_H

#include <asset/Assets.hpp>
#include <scene/Node.hpp>
#include <asset/serialization/Serialization.hpp>
#include <core/Containers.hpp>

#include <Types.hpp>

namespace hyperion::v2 {

class FBOMModelLoader : public AssetLoader
{
public:
    virtual ~FBOMModelLoader() = default;

    virtual LoadedAsset LoadAsset(LoaderState &state) const override;
};

} // namespace hyperion::v2

#endif