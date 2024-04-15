/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_V2_FONT_FACE_LOADER_H
#define HYPERION_V2_FONT_FACE_LOADER_H

#include <asset/Assets.hpp>

#include <scene/Node.hpp>

#include <core/Containers.hpp>

#include <rendering/font/FontFace.hpp>

#include <Types.hpp>

namespace hyperion::v2 {

class FontFaceLoader : public AssetLoader
{
public:
    virtual ~FontFaceLoader() = default;

    virtual LoadedAsset LoadAsset(LoaderState &state) const override;
};

} // namespace hyperion::v2

#endif