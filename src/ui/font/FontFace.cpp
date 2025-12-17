/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <UIPch.hpp>

#include <ui/font/FontEngine.hpp>
#include <ui/font/FontFace.hpp>

#ifdef HYP_FREETYPE

#include <ft2build.h>
#include FT_FREETYPE_H

#endif

namespace hyperion {

HYP_DECLARE_LOG_CHANNEL(Font);

FontFace::FontFace(FontEngine::Backend backend, const FilePath& path)
    : m_face(nullptr)
{
#ifdef HYP_FREETYPE
    if (FT_New_Face(backend, path.Data(), 0, &m_face))
    {
        m_face = nullptr;
        return;
    }

    RequestPixelSizes(0, 10);
#endif
}

FontFace::FontFace(FontFace&& other) noexcept
    : m_face(other.m_face)
{
    other.m_face = nullptr;
}

FontFace& FontFace::operator=(FontFace&& other) noexcept
{
    if (this != &other)
    {
        m_face = other.m_face;
        other.m_face = nullptr;
    }

    return *this;
}

FontFace::~FontFace()
{
#ifdef HYP_FREETYPE
    if (m_face != nullptr)
    {
        FT_Done_Face(m_face);
    }
#endif
}

void FontFace::SetGlyphSize(int ptW, int ptH, int screenWidth, int screenHeight)
{
#ifdef HYP_FREETYPE
    int error = FT_Set_Char_Size(m_face, ptW * 64, ptH * 64, screenWidth, screenHeight);
    if (error)
    {
        HYP_LOG(Font, Error, "Error! could not set the height of fontface to {}, {}", ptW, ptH);
    }
#endif
}

void FontFace::RequestPixelSizes(int width, int height)
{
#ifdef HYP_FREETYPE
    if (FT_Set_Pixel_Sizes(m_face, width, 64 * height))
    {
        HYP_LOG(Font, Error, "Could not set the height of fontface to {}, {}", width, height);
    }
#endif
}

FontFace::GlyphIndex FontFace::GetGlyphIndex(WChar toFind)
{
#ifdef HYP_FREETYPE
    Assert(m_face != nullptr);
    return FT_Get_Char_Index(m_face, toFind);
#else
    return -1;
#endif
}

FontEngine::Font FontFace::GetFace()
{
    return m_face;
}

} // namespace hyperion
