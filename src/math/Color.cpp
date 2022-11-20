#include <math/Color.hpp>
#include <math/MathUtil.hpp>
#include <util/ByteUtil.hpp>

namespace hyperion {

Color::Color()
    : value(0)
{
}

Color::Color(UInt32 hex)
    : value(hex)
{
}

Color::Color(Float r, Float g, Float b, Float a)
    : value(ByteUtil::PackColorU32(Vector4(r, g, b, a)))
{
}

Color::Color(Float rgba)
    : value(ByteUtil::PackColorU32(Vector4(rgba, rgba, rgba, rgba)))
{
}

Color::Color(const Color &other)
    : value(other.value)
{
}

Color::Color(const Vector4 &vec)
    : value(ByteUtil::PackColorU32(vec))
{
}

Color &Color::operator=(const Color &other)
{
    value = other.value;    

    return *this;
}

Color Color::operator+(const Color &other) const
{
    return Color(
        GetRed() + other.GetRed(),
        GetGreen() + other.GetGreen(),
        GetBlue() + other.GetBlue(),
        GetAlpha() + other.GetAlpha()
    );
}

Color &Color::operator+=(const Color &other)
{
    SetRed(GetRed() + other.GetRed());
    SetGreen(GetGreen() + other.GetGreen());
    SetBlue(GetBlue() + other.GetBlue());
    SetAlpha(GetAlpha() + other.GetAlpha());

    return *this;
}

Color Color::operator-(const Color &other) const
{
    return Color(
        GetRed() - other.GetRed(),
        GetGreen() - other.GetGreen(),
        GetBlue() - other.GetBlue(),
        GetAlpha() - other.GetAlpha()
    );
}

Color &Color::operator-=(const Color &other)
{
    SetRed(GetRed() - other.GetRed());
    SetGreen(GetGreen() - other.GetGreen());
    SetBlue(GetBlue() - other.GetBlue());
    SetAlpha(GetAlpha() - other.GetAlpha());

    return *this;
}

Color Color::operator*(const Color &other) const
{
    return Color(
        GetRed() * other.GetRed(),
        GetGreen() * other.GetGreen(),
        GetBlue() * other.GetBlue(),
        GetAlpha() * other.GetAlpha()
    );
}

Color &Color::operator*=(const Color &other)
{
    SetRed(GetRed() * other.GetRed());
    SetGreen(GetGreen() * other.GetGreen());
    SetBlue(GetBlue() * other.GetBlue());
    SetAlpha(GetAlpha() * other.GetAlpha());

    return *this;
}

Color Color::operator/(const Color &other) const
{
    return Color(
        GetRed() / MathUtil::Max(other.GetRed(), MathUtil::epsilon<Float>),
        GetGreen() / MathUtil::Max(other.GetGreen(), MathUtil::epsilon<Float>),
        GetBlue() / MathUtil::Max(other.GetBlue(), MathUtil::epsilon<Float>),
        GetAlpha() / MathUtil::Max(other.GetAlpha(), MathUtil::epsilon<Float>)
    );
}

Color &Color::operator/=(const Color &other)
{
    SetRed(GetRed() / MathUtil::Max(other.GetRed(), MathUtil::epsilon<Float>));
    SetGreen(GetGreen() / MathUtil::Max(other.GetGreen(), MathUtil::epsilon<Float>));
    SetBlue(GetBlue() / MathUtil::Max(other.GetBlue(), MathUtil::epsilon<Float>));
    SetAlpha(GetAlpha() / MathUtil::Max(other.GetAlpha(), MathUtil::epsilon<Float>));

    return *this;
}

bool Color::operator==(const Color &other) const
{
    return value == other.value;
}

bool Color::operator!=(const Color &other) const
{
    return value != other.value;
}

Color &Color::Lerp(const Color &to, Float amt)
{
    SetRed(MathUtil::Lerp(GetRed(), to.GetRed(), amt));
    SetGreen(MathUtil::Lerp(GetGreen(), to.GetGreen(), amt));
    SetBlue(MathUtil::Lerp(GetBlue(), to.GetBlue(), amt));
    SetAlpha(MathUtil::Lerp(GetAlpha(), to.GetAlpha(), amt));

    return *this;
}

std::ostream &operator<<(std::ostream &out, const Color &color) // output
{
    out << "[" << color.GetRed() << ", " << color.GetGreen() << ", " << color.GetBlue() << ", " << color.GetAlpha() << "]";
    return out;
}

} // namespace hyperion
