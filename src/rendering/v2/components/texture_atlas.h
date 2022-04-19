//
// Created by ethan on 4/5/22.
//

#ifndef HYPERION_TEXTURE_ATLAS_H
#define HYPERION_TEXTURE_ATLAS_H

#include "texture.h"
#include "framebuffer.h"
#include <vector>

namespace hyperion::v2 {

class TextureAtlas : public EngineComponentBase<STUB_CLASS(TextureAtlas)> {
public:
    using OffsetIndex = uint16_t;
    struct Offset {
        int32_t x, y;
        int32_t width, height;
    };

    TextureAtlas(Extent2D extent,
                 Image::InternalFormat format,
                 Image::FilterMode filter_mode,
                 Image::WrapMode wrap_mode);
    TextureAtlas(Extent2D extent);

    void Init(Engine *engine);
    void SetOffsets(const std::vector<Offset> &offsets) { m_offsets = offsets; }
    Offset GetOffset(OffsetIndex index) {
        if (index > m_offsets.size()) {
            DebugLog(LogType::Error, "Offset (%u) is out of bounds of the texture atlas!\n", index);
            return Offset { 0, 0, 0, 0 };
        }
        return m_offsets[index];
    }
    void BlitTexture(Engine *engine, Offset &dst_offset, Ref<Texture> &&src_texture, Offset &src_offset);

    inline Texture *GetTexture() const { return m_texture.ptr; }

    ~TextureAtlas();

private:
    void InternalBlitTexture(Engine *engine, Offset &dst_offset, Texture *src_texture, Offset &src_offset);
    struct QueueItem {
        Offset dst_offset;
        Offset src_offset;
        Texture *texture;
    };
    std::vector<QueueItem> m_enqueued;

    Extent2D m_extent;
    Image::InternalFormat m_format;
    Image::FilterMode m_filter_mode;
    Image::WrapMode m_wrap_mode;

    std::vector<Offset> m_offsets;
    Ref<Texture> m_texture;
};

}

#endif //HYPERION_TEXTURE_ATLAS_H
