#ifndef VECTOR2_H
#define VECTOR2_H

#include "../HashCode.hpp"
#include "../Util.hpp"

#include <util/Defines.hpp>
#include <Types.hpp>

#include <ostream>
#include <cmath>

#include "Vector3.hpp"

namespace hyperion {

class Vector4;

class Vector2
{
    friend std::ostream &operator<<(std::ostream &out, const Vector2 &vec);
public:
    static constexpr UInt size = 2;

    union {
        struct { float x, y; };
        float values[2];
    };

    Vector2();
    Vector2(float x, float y);
    Vector2(float xy);
    Vector2(const Vector2 &other);

    //! \brief Consturct a Vector2 from a Vector3, discarding y.
    Vector2(const Vector3 &other);

    //! \brief Consturct a Vector2 from a Vector4, discarding y and w.
    Vector2(const Vector4 &other);

    float GetX() const     { return x; }
    float &GetX()          { return x; }
    Vector2 &SetX(float x) { this->x = x; return *this; }
    float GetY() const     { return y; }
    float &GetY()          { return y; }
    Vector2 &SetY(float y) { this->y = y; return *this; }
    
    constexpr float operator[](UInt index) const
        { return values[index]; }

    constexpr float &operator[](UInt index)
        { return values[index]; }

    explicit operator bool() const
        { return Sum() != 0.0f; }

    Vector2 &operator=(const Vector2 &other);
    Vector2 operator+(const Vector2 &other) const;
    Vector2 &operator+=(const Vector2 &other);
    Vector2 operator-(const Vector2 &other) const;
    Vector2 &operator-=(const Vector2 &other);
    Vector2 operator*(const Vector2 &other) const;
    Vector2 &operator*=(const Vector2 &other);
    Vector2 operator/(const Vector2 &other) const;
    Vector2 &operator/=(const Vector2 &other);
    bool operator==(const Vector2 &other) const;
    bool operator!=(const Vector2 &other) const;
    Vector2 operator-() const { return operator*(-1.0f); }

    bool operator<(const Vector2 &other) const
        { return std::tie(x, y) < std::tie(other.x, other.y); }

    constexpr float LengthSquared() const { return x * x + y * y; }
    float Length() const { return std::sqrt(LengthSquared()); }

    constexpr float Avg() const { return (x + y) / 2.0f; }
    constexpr float Sum() const { return x + y; }
    float Max() const;
    float Min() const;

    float Distance(const Vector2 &other) const;
    float DistanceSquared(const Vector2 &other) const;

    Vector2 &Normalize();
    Vector2 &Lerp(const Vector2 &to, const float amt);

    static Vector2 Abs(const Vector2 &);
    static Vector2 Round(const Vector2 &);
    static Vector2 Clamp(const Vector2 &, float min, float max);
    static Vector2 Min(const Vector2 &a, const Vector2 &b);
    static Vector2 Max(const Vector2 &a, const Vector2 &b);

    static Vector2 Zero();
    static Vector2 One();
    static Vector2 UnitX();
    static Vector2 UnitY();

    HashCode GetHashCode() const
    {
        HashCode hc;

        hc.Add(x);
        hc.Add(y);

        return hc;
    }
};

static_assert(sizeof(Vector2) == sizeof(float) * 2, "sizeof(Vector2) must be equal to sizeof(float) * 2");

} // namespace hyperion

HYP_DEF_STL_HASH(hyperion::Vector2);

#endif
