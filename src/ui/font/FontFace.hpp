/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <ui/font/FontEngine.hpp>

#include <core/Defines.hpp>
#include <core/filesystem/FsUtil.hpp>

#include <core/Constants.hpp>
#include <core/Types.hpp>

namespace hyperion {

class FontFace
{
public:
    using WChar = uint32;
    using GlyphIndex = uint32;

    FontFace() = default;

    HYP_API FontFace(FontEngine::Backend backend, const FilePath& filePath);

    FontFace(const FontFace& other) = delete;
    FontFace& operator=(const FontFace& other) = delete;

    HYP_API FontFace(FontFace&& other) noexcept;
    HYP_API FontFace& operator=(FontFace&& other) noexcept;
    HYP_API ~FontFace();

    HYP_API void Init();

    HYP_API void RequestPixelSizes(int width, int height);
    HYP_API void SetGlyphSize(int ptW, int ptH, int screenWidth, int screenHeight);
    HYP_API GlyphIndex GetGlyphIndex(WChar toFind);
    HYP_API FontEngine::Font GetFace();

private:
    FontEngine::Font m_face;
};

} // namespace hyperion
