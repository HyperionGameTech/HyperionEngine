//
// Created by emd22 on 18/12/22.
//

#ifndef HYP_FONT_FACE_HPP
#define HYP_FONT_FACE_HPP

#include <string>
#include "FontEngine.hpp"

#include <core/Base.hpp>

#include <Constants.hpp>
#include <Types.hpp>

namespace hyperion::v2 {

class Engine;
class Face : public EngineComponentBase<STUB_CLASS(Face)>
{
public:
    using WChar = UInt32;
    using GlyphIndex = UInt;

    Face() = default;

    Face(FontEngine::Backend backend, const std::string &file_path);
    Face(const Face &other) = delete;
    Face &operator=(const Face &other) = delete;

    ~Face();

    void Init();

    void RequestPixelSizes(Int width, Int height);
    void SetGlyphSize(Int pt_w, Int pt_h, Int screen_width, Int screen_height);
    GlyphIndex GetGlyphIndex(WChar to_find);
    FontEngine::Font GetFace();

private:
    FontEngine::Font m_face = nullptr;

};

} // namespace hyperion::v2

#endif //HYP_FONT_FACE_HPP
