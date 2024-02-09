#include <math/Color.hpp>
#include <math/MathUtil.hpp>
#include <util/ByteUtil.hpp>

namespace hyperion {

Color::Color()
    : value(0)
{
}

Color::Color(uint32 hex)
    : value(hex)
{
}

Color::Color(float r, float g, float b, float a)
    : value(ByteUtil::PackColorU32(Vector4(r, g, b, a)))
{
}

Color::Color(float rgba)
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
        GetRed() / MathUtil::Max(other.GetRed(), MathUtil::epsilon_f),
        GetGreen() / MathUtil::Max(other.GetGreen(), MathUtil::epsilon_f),
        GetBlue() / MathUtil::Max(other.GetBlue(), MathUtil::epsilon_f),
        GetAlpha() / MathUtil::Max(other.GetAlpha(), MathUtil::epsilon_f)
    );
}

Color &Color::operator/=(const Color &other)
{
    SetRed(GetRed() / MathUtil::Max(other.GetRed(), MathUtil::epsilon_f));
    SetGreen(GetGreen() / MathUtil::Max(other.GetGreen(), MathUtil::epsilon_f));
    SetBlue(GetBlue() / MathUtil::Max(other.GetBlue(), MathUtil::epsilon_f));
    SetAlpha(GetAlpha() / MathUtil::Max(other.GetAlpha(), MathUtil::epsilon_f));

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

Color &Color::Lerp(const Color &to, float amt)
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
