/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

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
    using GlyphIndex = uint32;

    FontFace() = default;

    FontFace(FontEngine::Backend backend, const FilePath& filePath);

    FontFace(const FontFace& other) = delete;
    FontFace& operator=(const FontFace& other) = delete;

    FontFace(FontFace&& other) noexcept;
    FontFace& operator=(FontFace&& other) noexcept;
    ~FontFace();

    void RequestPixelSizes(int width, int height);
    void SetGlyphSize(int ptW, int ptH, int screenWidth, int screenHeight);
    GlyphIndex GetGlyphIndex(uint32 toFind);
    FontEngine::Font GetFace();

private:
    FontEngine::Font m_face;
};

} // namespace Hyperion
