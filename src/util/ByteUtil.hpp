#ifndef HYPERION_BYTE_UTIL_H
#define HYPERION_BYTE_UTIL_H

#include <math/MathUtil.hpp>
#include <math/Vector4.hpp>

#include <Types.hpp>

namespace hyperion {

class ByteUtil
{
public:

    static inline UInt32 PackFloatU32(Float value)
    {
        union {
            UInt32 u;
            Float f;
        };

        f = value;

        return u;
    }

    static inline Float UnpackFloatU32(UInt32 value)
    {
        union {
            UInt32 u;
            Float f;
        };

        u = value;

        return f;
    }

    static inline UInt32 PackColorU32(const Vector4 &vec)
    {
        union {
            UInt8 bytes[4];
            UInt32 result;
        };

        bytes[0] = MathUtil::Round<Float, UInt8>(MathUtil::Clamp(vec.values[0], 0.0f, 1.0f) * 255.0f);
        bytes[1] = MathUtil::Round<Float, UInt8>(MathUtil::Clamp(vec.values[1], 0.0f, 1.0f) * 255.0f);
        bytes[2] = MathUtil::Round<Float, UInt8>(MathUtil::Clamp(vec.values[2], 0.0f, 1.0f) * 255.0f);
        bytes[3] = MathUtil::Round<Float, UInt8>(MathUtil::Clamp(vec.values[3], 0.0f, 1.0f) * 255.0f);

        return result;
    }

    static inline Vector4 UnpackColor(UInt32 value)
    {
        return {
            Float(value & 0xff000000) / 255.0f,
            Float(value & 0x00ff0000) / 255.0f,
            Float(value & 0x0000ff00) / 255.0f,
            Float(value & 0x000000ff) / 255.0f
        };
    }
};

} // namespace hyperion

#endif