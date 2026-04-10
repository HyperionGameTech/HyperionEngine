#pragma once

#include <Core/FxTypes.hpp>
#include <format>

template <typename Type>
class FxVec2Base
{
public:
    constexpr FxVec2Base() = default;

    constexpr FxVec2Base(Type x, Type y) : X(x), Y(y) {}

    constexpr explicit FxVec2Base(Type scalar) : X(scalar), Y(scalar) {}

    FxVec2Base operator+(const FxVec2Base& other) const
    {
        FxVec2Base result = *this;
        result += other;
        return result;
    }

    FxVec2Base operator-(const FxVec2Base& other) const
    {
        FxVec2Base result = *this;
        result -= other;
        return result;
    }

    FxVec2Base operator*(const FxVec2Base& other) const
    {
        FxVec2Base result = *this;
        result *= other;
        return result;
    }

    FxVec2Base& operator+=(const FxVec2Base& other)
    {
        X += other.X;
        Y += other.Y;

        return *this;
    }

    FxVec2Base& operator-=(const FxVec2Base& other)
    {
        X -= other.X;
        Y -= other.Y;

        return *this;
    }

    FxVec2Base& operator*=(const FxVec2Base& other)
    {
        X *= other.X;
        Y *= other.Y;

        return *this;
    }

    FxVec2Base operator*(float scalar) const
    {
        FxVec2Base result = *this;
        result.X *= scalar;
        result.Y *= scalar;
        return result;
    }

    /**
     * @brief Checks if a vector is **exactly** equal to another vector. This does not account for floating point error.
     */
    bool operator==(const FxVec2Base<Type>& other) const { return (X == other.X && Y == other.Y); }

    FxVec2Base<Type>& operator=(const FxVec2Base<Type>& other)
    {
        X = other.X;
        Y = other.Y;
        return *this;
    }

    void Set(Type x, Type y)
    {
        X = x;
        Y = y;
    }

    Type GetX() const { return X; }
    Type GetY() const { return Y; }

    void SetX(Type x) { X = x; }
    void SetY(Type y) { Y = y; }

    Type Width() const { return GetX(); }
    Type Height() const { return GetY(); }

    static const FxVec2Base<Type> sZero;

public:
    union alignas(16)
    {
        float32 mData[2];

        struct
        {
            Type X, Y;
        };
    };
};

// Declaration of Vec2::sZero
template <typename Type>
const FxVec2Base<Type> FxVec2Base<Type>::sZero = FxVec2Base<Type>(0, 0);

using FxVec2f = FxVec2Base<float32>;
using FxVec2d = FxVec2Base<float64>;

using FxVec2i = FxVec2Base<int32>;
using FxVec2u = FxVec2Base<uint32>;


template <>
struct std::formatter<FxVec2i>
{
    auto parse(format_parse_context& ctx) { return ctx.begin(); }

    auto format(const FxVec2i& obj, std::format_context& ctx) const
    {
        return std::format_to(ctx.out(), "({}, {})", obj.X, obj.Y);
    }
};

template <>
struct std::formatter<FxVec2u>
{
    auto parse(format_parse_context& ctx) { return ctx.begin(); }

    auto format(const FxVec2u& obj, std::format_context& ctx) const
    {
        return std::format_to(ctx.out(), "({}, {})", obj.X, obj.Y);
    }
};

template <>
struct std::formatter<FxVec2f>
{
    auto parse(format_parse_context& ctx) { return ctx.begin(); }

    auto format(const FxVec2f& obj, std::format_context& ctx) const
    {
        return std::format_to(ctx.out(), "({:.04}, {:.04})", obj.X, obj.Y);
    }
};


template <>
struct std::formatter<FxVec2d>
{
    auto parse(format_parse_context& ctx) { return ctx.begin(); }

    auto format(const FxVec2d& obj, std::format_context& ctx) const
    {
        return std::format_to(ctx.out(), "({:.04}, {:.04})", obj.X, obj.Y);
    }
};
