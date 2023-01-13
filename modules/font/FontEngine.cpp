//
// Created by emd22 on 18/12/22.
//

#include "FontEngine.hpp"
#include "Face.hpp"

#include <iostream>

namespace hyperion::v2::font {

#include <ft2build.h>
#include FT_FREETYPE_H

FontEngine::FontEngine()
        : m_backend(nullptr)
{
    if (FT_Init_FreeType(&m_backend)) {
        std::cout << "Error! Cannot start FreeType engine.\n";
        m_backend = nullptr;
        return;
    }
}

FontEngine::Backend FontEngine::GetFontBackend()
{
    return m_backend;
}

Face FontEngine::LoadFont(const std::string &path)
{
    return Face(GetFontBackend(), path);
}


FontEngine::~FontEngine()
{
    FT_Done_FreeType(m_backend);
}

}; // namespace hyperion::v2::font