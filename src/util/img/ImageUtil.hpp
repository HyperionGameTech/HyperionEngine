/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_IMAGE_UTIL_H
#define HYPERION_IMAGE_UTIL_H

#include <math/MathUtil.hpp>
#include <Types.hpp>

namespace hyperion {

class ImageUtil {
public:
    static inline
    void ConvertBPP(
        uint width, uint height, uint depth,
        uint8 in_bpp, uint8 out_bpp,
        const ubyte * const in_bytes,
        ubyte *out_bytes
    )
    {
        const auto min_bpp = MathUtil::Min(in_bpp, out_bpp);

        for (uint x = 0; x < width; x++) {
            for (uint y = 0; y < height; y++) {
                for (uint z = 0; z < depth; z++) {
                    const uint idx = x * height * depth + y * depth + z;
                    const uint in_index = idx * in_bpp;
                    const uint out_index = idx * out_bpp;

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
