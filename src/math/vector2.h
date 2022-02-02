#ifndef VECTOR2_H
#define VECTOR2_H

#include "../hash_code.h"

#include <ostream>
#include <cmath>

namespace hyperion {

class Vector2 {
    friend std::ostream &operator<<(std::ostream &out, const Vector2 &vec);
public:
    float x, y;

    Vector2();
    Vector2(float x, float y);
    Vector2(float xy);
    Vector2(const Vector2 &other);

    inline float GetX() const { return x; }
    inline float &GetX() { return x; }
    inline Vector2 &SetX(float x) { this->x = x; return *this; }
    inline float GetY() const { return y; }
    inline float &GetY() { return y; }
    inline Vector2 &SetY(float y) { this->y = y; return *this; }

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
    inline Vector2 operator-() const { return operator*(-1.0f); }

    float Length() const;
    float LengthSquared() const;

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

    inline HashCode GetHashCode() const
    {
        HashCode hc;

        hc.Add(x);
        hc.Add(y);

        return hc;
    }
};

} // namespace hyperion

#endif
