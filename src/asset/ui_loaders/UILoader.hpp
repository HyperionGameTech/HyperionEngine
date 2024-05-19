/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_UI_LOADER_HPP
#define HYPERION_UI_LOADER_HPP

#include <asset/Assets.hpp>

#include <Types.hpp>

namespace hyperion {

class UILoader : public AssetLoader
{
public:
    virtual ~UILoader() = default;

    virtual LoadedAsset LoadAsset(LoaderState &state) const override;
};

} // namespace hyperion

#endif