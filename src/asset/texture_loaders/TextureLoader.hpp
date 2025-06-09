/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_TEXTURE_LOADER_HPP
#define HYPERION_TEXTURE_LOADER_HPP

#include <asset/Assets.hpp>
#include <scene/Node.hpp>

#include <Types.hpp>

namespace hyperion {

HYP_CLASS(NoScriptBindings)
class TextureLoader : public AssetLoaderBase
{
    HYP_OBJECT_BODY(TextureLoader);
    
public:
    virtual ~TextureLoader() = default;

    virtual AssetLoadResult LoadAsset(LoaderState& state) const override;
};

} // namespace hyperion

#endif