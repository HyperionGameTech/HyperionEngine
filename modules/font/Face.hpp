//
// Created by emd22 on 18/12/22.
//

#ifndef HYP_FONT_FACE_HPP
#define HYP_FONT_FACE_HPP

#include <string>
#include "FontEngine.hpp"

#include <Types.hpp>

namespace hyperion::v2::font {

class Face
{
public:
    using WChar = UInt32;
    using GlyphIndex = UInt;

    Face(FontEngine::Backend backend, const std::string &path);

    void RequestPixelSizes(Int width, Int height);
    void SetGlyphSize(Int pt_w, Int pt_h, Int screen_width, Int screen_height);
    GlyphIndex GetGlyphIndex(WChar to_find);
    FontEngine::Font GetFace();

    ~Face() = default;

private:
    FontEngine::Font m_face;

};

} // namespace hyperion::v2::font

#endif //HYP_FONT_FACE_HPP
