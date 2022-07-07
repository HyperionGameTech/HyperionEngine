#ifndef HYPERION_BYTE_UTIL_H
#define HYPERION_BYTE_UTIL_H

#include <math/MathUtil.hpp>
#include <math/Vector4.hpp>

namespace hyperion {

class ByteUtil {
public:

    static inline uint32_t PackColorU32(const Vector4 &vec)
    {
        union {
            uint8_t  bytes[4];
            uint32_t result;
        };

        bytes[0] = MathUtil::Round<float, uint8_t>(MathUtil::Clamp(vec.values[0], 0.0f, 1.0f) * 255.0f);
        bytes[1] = MathUtil::Round<float, uint8_t>(MathUtil::Clamp(vec.values[1], 0.0f, 1.0f) * 255.0f);
        bytes[2] = MathUtil::Round<float, uint8_t>(MathUtil::Clamp(vec.values[2], 0.0f, 1.0f) * 255.0f);
        bytes[3] = MathUtil::Round<float, uint8_t>(MathUtil::Clamp(vec.values[3], 0.0f, 1.0f) * 255.0f);

        return result;
    }

    static inline Vector4 UnpackColor(uint32_t value)
    {
        return {
            float(value & 0xff000000) / 255.0f,
            float(value & 0x00ff0000) / 255.0f,
            float(value & 0x0000ff00) / 255.0f,
            float(value & 0x000000ff) / 255.0f
        };
    }
};

} // namespace hyperion

#endif