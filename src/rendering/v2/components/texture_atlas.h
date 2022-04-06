//
// Created by ethan on 4/5/22.
//

#ifndef HYPERION_TEXTURE_ATLAS_H
#define HYPERION_TEXTURE_ATLAS_H

#include "texture.h"
#include "framebuffer.h"

namespace hyperion::v2 {

class TextureAtlas {
public:
    struct Offset {
        uint16_t x;
        uint16_t y;
    };


    TextureAtlas(uint32_t width, uint32_t height);
    Blit(const Offset &offset, Texture2D texture);


    Texture2D m_texture;
    Framebuffer m_framebuffer;
};


}

#endif //HYPERION_TEXTURE_ATLAS_H
