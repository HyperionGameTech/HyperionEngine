/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYP_FONT_FONTENGINE_HPP
#define HYP_FONT_FONTENGINE_HPP

#include <core/filesystem/FilePath.hpp>

#include <core/logging/LoggerFwd.hpp>

struct FT_LibraryRec_;
struct FT_FaceRec_;
struct FT_GlyphSlotRec_;

namespace hyperion {

HYP_DECLARE_LOG_CHANNEL(Font);

class FontFace;

class FontEngine
{
public:
    static FontEngine& GetInstance();

    using Backend = FT_LibraryRec_*;
    using Font = FT_FaceRec_*;
    using Glyph = FT_GlyphSlotRec_*;

    FontEngine();
    ~FontEngine();

    HYP_NODISCARD
    FontFace
    LoadFont(const FilePath& path);

    HYP_NODISCARD
    Backend
    GetFontBackend();

private:
    Backend m_backend;
};

} // namespace hyperion

#endif // HYP_FONT_FONTENGINE_HPP
