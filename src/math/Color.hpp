#ifndef HYPERION_COLOR_HPP
#define HYPERION_COLOR_HPP

#include <math/Vector4.hpp>

#include <HashCode.hpp>
#include <util/Defines.hpp>
#include <Types.hpp>

namespace hyperion {

class alignas(UInt32) Color
{
public:
    static constexpr UInt size = 4;

private:
    friend std::ostream &operator<<(std::ostream &out, const Color &color);

    union {
        UInt32 value;
        UByte bytes[size];
    };

public:
    Color();
    Color(UInt32 hex);
    Color(Float r, Float g, Float b, Float a = 1.0f);
    explicit Color(Float rgba);
    Color(const Color &other);
    Color(const Vector4 &vec);

    HYP_FORCE_INLINE Float GetRed() const
        { return Float(bytes[0]) / 255.0f; }

    HYP_FORCE_INLINE Color &SetRed(Float red)
        { bytes[0] = static_cast<UByte>(red * 255.0f); return *this; }

    HYP_FORCE_INLINE Float GetGreen() const
        { return Float(bytes[1]) / 255.0f; }

    HYP_FORCE_INLINE Color &SetGreen(Float green)
        { bytes[1] = static_cast<UByte>(green * 255.0f); return *this; }

    HYP_FORCE_INLINE Float GetBlue() const
        { return Float(bytes[2]) / 255.0f; }

    HYP_FORCE_INLINE Color &SetBlue(Float blue)
        { bytes[2] = static_cast<UByte>(blue * 255.0f); return *this; }

    HYP_FORCE_INLINE Float GetAlpha() const
        { return Float(bytes[3]) / 255.0f; }

    HYP_FORCE_INLINE Color &SetAlpha(Float alpha)
        { bytes[3] = static_cast<UByte>(alpha * 255.0f); return *this; }
    
    constexpr Float operator[](UInt index) const
        { return Float(bytes[index]) / 255.0f; }

    Color &operator=(const Color &other);
    Color operator+(const Color &other) const;
    Color &operator+=(const Color &other);
    Color operator-(const Color &other) const;
    Color &operator-=(const Color &other);
    Color operator*(const Color &other) const;
    Color &operator*=(const Color &other);
    Color operator/(const Color &other) const;
    Color &operator/=(const Color &other);
    bool operator==(const Color &other) const;
    bool operator!=(const Color &other) const;

    HYP_FORCE_INLINE bool operator<(const Color &other) const
        { return value < other.value; }

    HYP_FORCE_INLINE explicit operator UInt32() const
        { return value; }

    HYP_FORCE_INLINE explicit operator Vector4() const
        { return Vector4(GetRed(), GetGreen(), GetBlue(), GetAlpha()); }

    HYP_FORCE_INLINE UInt32 Packed() const
        { return value; }

    Color &Lerp(const Color &to, Float amt);

    HashCode GetHashCode() const
    {
        HashCode hc;

        hc.Add(value);

        return hc;
    }
};

static_assert(sizeof(Color) == sizeof(UInt32), "sizeof(Color) must be equal to sizeof uint32");
static_assert(alignof(Color) == alignof(UInt32), "alignof(Color) must be equal to alignof uint32");

} // namespace hyperion

HYP_DEF_STL_HASH(hyperion::Color);

#endif
