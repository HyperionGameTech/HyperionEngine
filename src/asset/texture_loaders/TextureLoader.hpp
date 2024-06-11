/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_TEXTURE_LOADER_HPP
#define HYPERION_TEXTURE_LOADER_HPP

#include <asset/Assets.hpp>
#include <scene/Node.hpp>

#include <Types.hpp>
#include <rendering/Texture.hpp>

namespace hyperion {

class TextureLoader : public AssetLoader
{
public:
    virtual ~TextureLoader() = default;

    virtual LoadedAsset LoadAsset(LoaderState &state) const override;
};

} // namespace hyperion

#endif