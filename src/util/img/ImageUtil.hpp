/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_IMAGE_UTIL_HPP
#define HYPERION_IMAGE_UTIL_HPP

#include <core/math/MathUtil.hpp>
#include <Types.hpp>

namespace hyperion {

class ImageUtil {
public:
    static inline
    void ConvertBPP(
        uint32 width, uint32 height, uint32 depth,
        uint8 in_bpp, uint8 out_bpp,
        const ubyte * const in_bytes,
        ubyte *out_bytes
    )
    {
        const auto min_bpp = MathUtil::Min(in_bpp, out_bpp);

        for (uint32 x = 0; x < width; x++) {
            for (uint32 y = 0; y < height; y++) {
                for (uint32 z = 0; z < depth; z++) {
                    const uint32 idx = x * height * depth + y * depth + z;
                    const uint32 in_index = idx * in_bpp;
                    const uint32 out_index = idx * out_bpp;

                    for (uint8 i = 0; i < min_bpp; i++) {
                        out_bytes[out_index + i] = in_bytes[in_index + i];
                    }

                    for (uint8 i = min_bpp; i < out_bpp; i++) {
                        out_bytes[out_index + i] = 255;
                    }
                }
            }
        }
    }
};

} // namespace hyperion

#endif
