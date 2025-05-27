/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_FONT_FACE_LOADER_HPP
#define HYPERION_FONT_FACE_LOADER_HPP

#include <asset/Assets.hpp>

#include <scene/Node.hpp>

#include <rendering/font/FontFace.hpp>

#include <Types.hpp>

namespace hyperion {

class FontFaceLoader : public AssetLoaderBase
{
public:
    virtual ~FontFaceLoader() = default;

    virtual AssetLoadResult LoadAsset(LoaderState& state) const override;
};

} // namespace hyperion

#endif