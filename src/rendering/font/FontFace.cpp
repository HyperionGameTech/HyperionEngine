#include <rendering/font/FontEngine.hpp>
#include <rendering/font/FontFace.hpp>

#ifdef HYP_FREETYPE

#include <ft2build.h>
#include FT_FREETYPE_H

#endif

namespace hyperion::v2 {

FontFace::FontFace(FontEngine::Backend backend, const FilePath &path)
    : m_face(nullptr)
{
#ifdef HYP_FREETYPE
    if (FT_New_Face(backend, path.Data(), 0, &m_face)) {
        m_face = nullptr;
        return;
    }

    RequestPixelSizes(0, 48);
#endif
}

FontFace::FontFace(FontFace &&other) noexcept
    : m_face(other.m_face)
{
    other.m_face = nullptr;
}

FontFace &FontFace::operator=(FontFace &&other) noexcept
{
    if (this != &other) {
        m_face = other.m_face;
        other.m_face = nullptr;
    }

    return *this;
}

FontFace::~FontFace()
{
}

void FontFace::SetGlyphSize(int pt_w, int pt_h, int screen_width, int screen_height)
{
#ifdef HYP_FREETYPE
    int error = FT_Set_Char_Size(m_face, pt_w * 64, pt_h * 64, screen_width, screen_height);
    if (error) {
        DebugLog(LogType::Error, "Error could not set glyph size\n");
    }
#endif
}

void FontFace::RequestPixelSizes(int width, int height)
{
#ifdef HYP_FREETYPE
    if (FT_Set_Pixel_Sizes(m_face, width, height)) {
        DebugLog(LogType::Error, "Error! could not set the height of fontface to %u, %u\n", width, height);
    }
#endif
}

FontFace::GlyphIndex FontFace::GetGlyphIndex(WChar to_find)
{
#ifdef HYP_FREETYPE
    AssertThrow(m_face != nullptr);
    return FT_Get_Char_Index(m_face, to_find);
#else
    return -1;
#endif
}

FontEngine::Font FontFace::GetFace()
{
    return m_face;
}

} // namespace hyperion::v2::font