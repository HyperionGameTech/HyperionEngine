/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYP_FONT_FONTENGINE_HPP
#define HYP_FONT_FONTENGINE_HPP

#include <util/fs/FsUtil.hpp>

struct FT_LibraryRec_;
struct FT_FaceRec_;
struct FT_GlyphSlotRec_;

namespace hyperion::v2 {

class FontFace;

class FontEngine
{
public:
    static FontEngine &GetInstance();

    using Backend = FT_LibraryRec_ *;
    using Font = FT_FaceRec_ *;
    using Glyph = FT_GlyphSlotRec_ *;

    FontEngine();
    ~FontEngine();

    FontFace LoadFont(const FilePath &path);

    Backend GetFontBackend();

private:
    Backend m_backend;
};

} // namespace hyperion::v2


#endif //HYP_FONT_FONTENGINE_HPP
