#ifndef HYPERION_V2_DEFAULT_FORMATS_H
#define HYPERION_V2_DEFAULT_FORMATS_H

namespace hyperion::v2 {

enum TextureFormatDefault {
    TEXTURE_FORMAT_DEFAULT_NONE         = 0,
    TEXTURE_FORMAT_DEFAULT_COLOR        = 1 << 0,
    TEXTURE_FORMAT_DEFAULT_DEPTH        = 1 << 1,
    TEXTURE_FORMAT_DEFAULT_GBUFFER      = 1 << 2,
    TEXTURE_FORMAT_DEFAULT_GBUFFER_8BIT = 1 << 3,
    TEXTURE_FORMAT_DEFAULT_NORMALS      = 1 << 4,
    TEXTURE_FORMAT_DEFAULT_UV           = 1 << 5,
    TEXTURE_FORMAT_DEFAULT_STORAGE      = 1 << 6
};

} // namespace hyperion::v2

#endif