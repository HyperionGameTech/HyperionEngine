/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <Core/FileSystem/FilePath.hpp>

#include <Core/Logging/LoggerFwd.hpp>

struct FT_LibraryRec_;
struct FT_FaceRec_;
struct FT_GlyphSlotRec_;

namespace Hyperion {

ENGINE_API HYP_DECLARE_LOG_CHANNEL(Font);

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

    HYP_NODISCARD FontFace LoadFont(const FilePath& path);

    HYP_NODISCARD Backend GetFontBackend();

private:
    Backend m_backend;
};

} // namespace Hyperion
