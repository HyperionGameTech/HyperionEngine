/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <asset/AssetLoader.hpp>

#include <core/Types.hpp>

namespace Hyperion {

HYP_CLASS(NoScriptBindings)
class FBOMModelLoader : public AssetLoaderBase
{
    HYP_OBJECT_BODY(FBOMModelLoader);

public:
    virtual ~FBOMModelLoader() = default;

    virtual AssetLoadResult LoadAsset(LoaderState& state) const override;
};

} // namespace Hyperion
