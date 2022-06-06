#ifndef HYPERION_IMAGE_UTIL_H
#define HYPERION_IMAGE_UTIL_H

#include <math/math_util.h>
#include <types.h>

namespace hyperion {

class ImageUtil {
public:
    static inline
    void ConvertBpp(
        size_t width, size_t height, size_t depth,
        UByte in_bpp, UByte out_bpp,
        const UByte *const in_bytes,
        UByte *out_bytes)
    {
        const UByte min_bpp = MathUtil::Min(in_bpp, out_bpp);

        for (size_t x = 0; x < width; x++) {
            for (size_t y = 0; y < height; y++) {
                for (size_t z = 0; z < depth; z++) {
                    const size_t idx = x * height * depth + y * depth + z; //((z * width * height) + (y * height) + x);
                    const size_t in_index  = idx * in_bpp;
                    const size_t out_index = idx * out_bpp;

                    for (UByte i = 0; i < min_bpp; i++) {
                        out_bytes[out_index + i] = in_bytes[in_index + i];
                    }

                    for (UByte i = min_bpp; i < out_bpp; i++) {
                        out_bytes[out_index + i] = 255;
                    }
                }
            }
        }
    }
};

} // namespace hyperion

#endif
