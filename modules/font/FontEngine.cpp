//
// Created by emd22 on 18/12/22.
//

#include "FontEngine.hpp"
#include "Face.hpp"

#include <iostream>

#include <system/Debug.hpp>

namespace hyperion::v2 {

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

hyperion::v2::Face FontEngine::LoadFont(const std::string &path)
{
    if (m_backend == nullptr) {
        DebugLog(LogType::Error, "Font backend system not initialized!\n");
    }
    return { GetFontBackend(), path };
}


FontEngine::~FontEngine()
{
    FT_Done_FreeType(m_backend);
    m_backend = nullptr;
}

}; // namespace hyperion::v2