/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <asset/AssetLoader.hpp>

#include <ui/font/FontFace.hpp>

#include <core/Types.hpp>

namespace Hyperion {

HYP_CLASS(NoScriptBindings)
class FontFaceLoader : public AssetLoaderBase
{
    HYP_OBJECT_BODY(FontFaceLoader);

public:
    virtual ~FontFaceLoader() = default;

    virtual AssetLoadResult LoadAsset(LoaderState& state) const override;
};

} // namespace Hyperion
