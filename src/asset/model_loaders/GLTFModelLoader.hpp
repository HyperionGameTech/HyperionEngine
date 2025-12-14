/* Copyright (c) 2025 No Tomorrow Games. All rights reserved. */

#pragma once

#include <asset/AssetLoader.hpp>

#include <core/Types.hpp>

namespace hyperion {

HYP_CLASS(NoScriptBindings)
class GLTFModelLoader : public AssetLoaderBase
{
    HYP_OBJECT_BODY(GLTFModelLoader);

public:
    GLTFModelLoader();
    virtual ~GLTFModelLoader() = default;

    virtual AssetLoadResult LoadAsset(LoaderState& state) const override;
};

} // namespace hyperion
