//
// Created by emd22 on 18/12/22.
//

#ifndef HYP_FONT_FONTENGINE_HPP
#define HYP_FONT_FONTENGINE_HPP

#include <string>


namespace hyperion::v2 {

class Face;

struct FT_LibraryRec_;
struct FT_FaceRec_;
struct FT_GlyphSlotRec_;

class FontEngine {
public:
    using Backend = FT_LibraryRec_ *;
    using Font = FT_FaceRec_ *;
    using Glyph = FT_GlyphSlotRec_ *;

    FontEngine();

    Face LoadFont(const std::string &path);

    Backend GetFontBackend();

    ~FontEngine();

private:
    Backend m_backend;
};

} // namespace hyperion::v2


#endif //HYP_FONT_FONTENGINE_HPP
