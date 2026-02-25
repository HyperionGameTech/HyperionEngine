/* Copyright (c) 2016-2026 Andrew J. MacDonald. All rights reserved. */

#pragma once

#include <asset/AssetLoader.hpp>

#include <Core/Types.hpp>

namespace Hyperion {

HYP_CLASS(NoScriptBindings)
class TextureLoader : public AssetLoaderBase
{
    HYP_OBJECT_BODY(TextureLoader);

public:
    virtual ~TextureLoader() = default;

    virtual AssetLoadResult LoadAsset(LoaderState& state) const override;
};

} // namespace Hyperion
