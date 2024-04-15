/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYP_FONT_FACE_HPP
#define HYP_FONT_FACE_HPP

#include <rendering/font/FontEngine.hpp>

#include <core/Base.hpp>

#include <core/Defines.hpp>
#include <util/fs/FsUtil.hpp>

#include <Constants.hpp>
#include <Types.hpp>

namespace hyperion::v2 {

class Engine;

class FontFace
{
public:
    using WChar = uint32;
    using GlyphIndex = uint;

    FontFace()                                                          = default;

    HYP_API FontFace(FontEngine::Backend backend, const FilePath &file_path);

    FontFace(const FontFace &other)                                     = delete;
    FontFace &operator=(const FontFace &other)                          = delete;

    HYP_API FontFace(FontFace &&other) noexcept;
    HYP_API FontFace &operator=(FontFace &&other) noexcept;
    HYP_API ~FontFace();

    HYP_API void Init();

    HYP_API void RequestPixelSizes(int width, int height);
    HYP_API void SetGlyphSize(int pt_w, int pt_h, int screen_width, int screen_height);
    HYP_API GlyphIndex GetGlyphIndex(WChar to_find);
    HYP_API FontEngine::Font GetFace();

private:
    FontEngine::Font m_face;
};

} // namespace hyperion::v2

#endif //HYP_FONT_FACE_HPP
