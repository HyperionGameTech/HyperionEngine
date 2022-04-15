//
// Created by ethan on 4/5/22.
//

#ifndef HYPERION_TEXTURE_ATLAS_H
#define HYPERION_TEXTURE_ATLAS_H

#include "texture.h"
#include "framebuffer.h"
#include <vector>

namespace hyperion::v2 {

class TextureAtlas {
public:
    using OffsetIndex = uint16_t;
    struct Offset {
        int32_t x, y;
        int32_t width, height;
    };

    TextureAtlas(uint32_t width, uint32_t height,
                 Image::InternalFormat format,
                 Image::FilterMode filter_mode,
                 Image::WrapMode wrap_mode);
    TextureAtlas(uint32_t width, uint32_t height);

    void Create(Engine *engine);
    void SetOffsets(const std::vector<Offset> &offsets) { m_offsets = offsets; }
    Offset GetOffset(OffsetIndex index) {
        if (index > m_offsets.size()) {
            DebugLog(LogType::Error, "Offset (%u) is out of bounds of the texture atlas!\n", index);
            return Offset { 0, 0, 0, 0 };
        }
        return m_offsets[index];
    }

    inline Texture2D *GetTexture() const { return m_texture.get(); }

    void BlitTexture(Engine *engine, Offset &dst_offset, Texture2D *src_texture, Offset &src_offset);

private:
    uint32_t m_width = 0;
    uint32_t m_height = 0;

    std::vector<Offset> m_offsets;
    std::unique_ptr<Texture2D> m_texture;
};


}

#endif //HYPERION_TEXTURE_ATLAS_H
