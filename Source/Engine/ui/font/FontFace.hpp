/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <ui/font/FontEngine.hpp>

#include <Core/Defines.hpp>
#include <Core/filesystem/FsUtil.hpp>

#include <Core/Constants.hpp>
#include <Core/Types.hpp>

namespace Hyperion {

class FontFace
{
public:
    using WChar = uint32;
    using GlyphIndex = uint32;

    FontFace() = default;

    FontFace(FontEngine::Backend backend, const FilePath& filePath);

    FontFace(const FontFace& other) = delete;
    FontFace& operator=(const FontFace& other) = delete;

    FontFace(FontFace&& other) noexcept;
    FontFace& operator=(FontFace&& other) noexcept;
    ~FontFace();

    void Init();

    void RequestPixelSizes(int width, int height);
    void SetGlyphSize(int ptW, int ptH, int screenWidth, int screenHeight);
    GlyphIndex GetGlyphIndex(WChar toFind);
    FontEngine::Font GetFace();

private:
    FontEngine::Font m_face;
};

} // namespace Hyperion
