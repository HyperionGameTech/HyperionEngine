#ifndef HYPERION_V2_DEFAULT_FORMATS_H
#define HYPERION_V2_DEFAULT_FORMATS_H

namespace hyperion::v2 {

enum TextureFormatDefault
{
    TEXTURE_FORMAT_DEFAULT_NONE         = 0,
    TEXTURE_FORMAT_DEFAULT_COLOR        = 1 << 0,
    TEXTURE_FORMAT_DEFAULT_DEPTH        = 1 << 1,
    TEXTURE_FORMAT_DEFAULT_RESERVED0    = 1 << 2,
    TEXTURE_FORMAT_DEFAULT_NORMALS      = 1 << 3,
    TEXTURE_FORMAT_DEFAULT_RESERVED1    = 1 << 4,
    TEXTURE_FORMAT_DEFAULT_STORAGE      = 1 << 5
};

} // namespace hyperion::v2

#endif