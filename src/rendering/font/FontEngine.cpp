/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <rendering/font/FontEngine.hpp>
#include <rendering/font/FontFace.hpp>

#include <system/Debug.hpp>

#ifdef HYP_FREETYPE

#include <ft2build.h>
#include FT_FREETYPE_H

#endif

namespace hyperion::v2 {

FontEngine &FontEngine::GetInstance()
{
    static FontEngine instance;

    return instance;
}

FontEngine::FontEngine()
        : m_backend(nullptr)
{
#ifdef HYP_FREETYPE
    if (FT_Init_FreeType(&m_backend)) {
        DebugLog(LogType::Error, "Error! Cannot start FreeType engine.\n");
        m_backend = nullptr;
        return;
    }
#endif
}

FontEngine::~FontEngine()
{
#ifdef HYP_FREETYPE
    if (m_backend != nullptr) {
        FT_Done_FreeType(m_backend);
        m_backend = nullptr;
    }
#endif
}

FontEngine::Backend FontEngine::GetFontBackend()
{
    return m_backend;
}

hyperion::v2::FontFace FontEngine::LoadFont(const FilePath &path)
{
    if (m_backend == nullptr) {
        DebugLog(LogType::Error, "Font backend system not initialized!\n");
    }

    return { GetFontBackend(), path };
}

} // namespace hyperion::v2