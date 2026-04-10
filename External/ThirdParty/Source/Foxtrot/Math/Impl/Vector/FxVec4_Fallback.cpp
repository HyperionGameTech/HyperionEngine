#include <Core/FxDefines.hpp>

#ifdef FX_NO_SIMD

#include <Math/FxVec4.hpp>

Vec4f::Vec4f(float32 x, float32 y, float32 z, float32 w) : X(x), Y(y), Z(z), W(w) {}

inline Vec4f::Vec4f(float32 scalar) : X(scalar), Y(scalar), Z(scalar), W(scalar) {}

inline Vec4f Vec4f::operator+(const Vec4f& other) const
{
    Vec4f result;

    result.X = X + other.X;
    result.Y = Y + other.Y;
    result.Z = Z + other.Z;
    result.W = W + other.W;

    return result;
}

inline Vec4f Vec4f::operator*(const Vec4f& other) const
{
    Vec4f result;

    result.X = X * other.X;
    result.Y = Y * other.Y;
    result.Z = Z * other.Z;
    result.W = W * other.W;

    return result;
}

inline Vec4f& Vec4f::operator+=(const Vec4f& other)
{
    X += other.X;
    Y += other.Y;
    Z += other.Z;
    W += other.W;

    return *this;
}

inline Vec4f& Vec4f::operator-=(const Vec4f& other)
{
    X -= other.X;
    Y -= other.Y;
    Z -= other.Z;
    W -= other.W;

    return *this;
}

inline Vec4f& Vec4f::operator*=(const Vec4f& other)
{
    X *= other.X;
    Y *= other.Y;
    Z *= other.Z;
    W *= other.W;

    return *this;
}

Vec4f& Vec4f::operator=(const Vec4f& other)
{
    X = other.X;
    Y = other.Y;
    Z = other.Z;
    W = other.W;

    return *this;
}

inline Vec4f Vec4f::MulAdd(const Vec4f& add_value, const Vec4f& mul_a, const Vec4f& mul_b)
{
    Vec4f result;
    result = add_value + (mul_a * mul_b);
    return result;
}

inline float32 Vec4f::GetX() const { return X; }

inline float32 Vec4f::GetY() const { return Y; }

inline float32 Vec4f::GetZ() const { return Z; }

inline float32 Vec4f::GetW() const { return W; }

inline void Vec4f::Load1(float32 scalar) noexcept { X = Y = Z = W = scalar; }

#endif // #ifdef FX_NO_SIMD
