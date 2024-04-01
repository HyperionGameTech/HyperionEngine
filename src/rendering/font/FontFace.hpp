#ifndef HYP_FONT_FACE_HPP
#define HYP_FONT_FACE_HPP

#include <rendering/font/FontEngine.hpp>

#include <core/Base.hpp>

#include <util/fs/FsUtil.hpp>

#include <Constants.hpp>
#include <Types.hpp>

namespace hyperion::v2 {

class Engine;

class FontFace
{
public:
    using WChar = uint32;
    using GlyphIndex = uint;

    FontFace()                                  = default;
    FontFace(FontEngine::Backend backend, const FilePath &file_path);
    FontFace(const FontFace &other)                 = delete;
    FontFace &operator=(const FontFace &other)      = delete;
    FontFace(FontFace &&other) noexcept;
    FontFace &operator=(FontFace &&other) noexcept;
    ~FontFace();

    void Init();

    void RequestPixelSizes(int width, int height);
    void SetGlyphSize(int pt_w, int pt_h, int screen_width, int screen_height);
    GlyphIndex GetGlyphIndex(WChar to_find);
    FontEngine::Font GetFace();

private:
    FontEngine::Font m_face;
};

} // namespace hyperion::v2

#endif //HYP_FONT_FACE_HPP
