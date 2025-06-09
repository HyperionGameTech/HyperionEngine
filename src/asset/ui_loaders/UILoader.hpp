/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_UI_LOADER_HPP
#define HYPERION_UI_LOADER_HPP

#include <asset/Assets.hpp>

#include <Types.hpp>

namespace hyperion {

HYP_CLASS(NoScriptBindings)
class UILoader : public AssetLoaderBase
{
    HYP_OBJECT_BODY(UILoader);
    
public:
    virtual ~UILoader() = default;

    virtual AssetLoadResult LoadAsset(LoaderState& state) const override;
};

} // namespace hyperion

#endif