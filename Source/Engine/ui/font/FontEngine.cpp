/* Copyright (c) 2026 Andrew J. MacDonald. All rights reserved. */

#include <UIPch.hpp>

#include <ui/font/FontEngine.hpp>
#include <ui/font/FontFace.hpp>

#include <Core/debug/Debug.hpp>

#ifdef HYP_FREETYPE

#include <ft2build.h>
#include FT_FREETYPE_H

#endif

namespace Hyperion {

FontEngine& FontEngine::GetInstance()
{
    static FontEngine instance;

    return instance;
}

FontEngine::FontEngine()
    : m_backend(nullptr)
{
#ifdef HYP_FREETYPE
    if (FT_Init_FreeType(&m_backend))
    {
        HYP_LOG(Font, Error, "Error! Cannot start FreeType engine.");
        m_backend = nullptr;
        return;
    }
#endif
}

FontEngine::~FontEngine()
{
#ifdef HYP_FREETYPE
    if (m_backend != nullptr)
    {
        FT_Done_FreeType(m_backend);
        m_backend = nullptr;
    }
#endif
}

FontEngine::Backend FontEngine::GetFontBackend()
{
    return m_backend;
}

Hyperion::FontFace FontEngine::LoadFont(const FilePath& path)
{
    if (m_backend == nullptr)
    {
        HYP_LOG(Font, Error, "Font backend system not initialized!");
    }

    return { GetFontBackend(), path };
}

} // namespace Hyperion
