//
// Created by emd22 on 18/12/22.
//

#include "FontEngine.hpp"
#include "Face.hpp"

#include <iostream>

namespace hyperion::v2 {

#include <ft2build.h>
#include FT_FREETYPE_H

Face::Face(FontEngine::Backend backend, const std::string &path)
        : EngineComponentBase(),
          m_face(nullptr)
{
    if (FT_New_Face(backend, path.c_str(), 0, &m_face)) {
        std::cout << "Error! could not load font face at '" << path << "'!\n";
        m_face = nullptr;
        return;
    }
}

Face::~Face()
{
    Teardown();
}

void Face::Init()
{
    if (IsInitCalled()) {
        return;
    }

    EngineComponentBase::Init();
}

void Face::SetGlyphSize(Int pt_w, Int pt_h, Int screen_width, Int screen_height)
{
    Int error = FT_Set_Char_Size(m_face, pt_w * 64, pt_h * 64, screen_width, screen_height);
    if (error) {
        std::cout << "Error could not set glyph size\n";
        // TODO: setup error system here
    }
}

void Face::RequestPixelSizes(Int width, Int height)
{
    if (FT_Set_Pixel_Sizes(m_face, width, height)) {
        std::cout << "Error! could not set the height of fontface to " << width << ", " << height << ".\n";
    }
}

Face::GlyphIndex Face::GetGlyphIndex(WChar to_find)
{
    return FT_Get_Char_Index(m_face, to_find);
}

FontEngine::Font Face::GetFace()
{
    return m_face;
}

} // namespace hyperion::v2::font