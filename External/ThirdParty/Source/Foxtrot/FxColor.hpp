#pragma once

#include <Core/FxTypes.hpp>

struct FxColor
{
public:
    // Colour definitions
    static const FxColor sNone;

    static const FxColor sWhite;
    static const FxColor sBlack;
    static const FxColor sRed;
    static const FxColor sBlue;
    static const FxColor sGreen;

public:
    FxColor() {}
    FxColor(uint32 value) : Value(value) {}
    FxColor(uint32 value, uint8 brightness)
    {
        Value = value;
        A = brightness;
    }

    static FX_FORCE_INLINE FxColor FromRGBA(uint8 r, uint8 g, uint8 b, uint8 a);
    static FX_FORCE_INLINE FxColor FromFloats(float32 rgba[4]);

public:
    union
    {
        struct
        {
            uint8 R, G, B, A;
        };

        uint32 Value;
    };
};

///////////////////////////
// Definitions
///////////////////////////

FX_FORCE_INLINE FxColor FxColor::FromRGBA(uint8 r, uint8 g, uint8 b, uint8 a)
{
    FxColor colour;
    colour.Value = ((static_cast<uint32>(a) << 24) | (static_cast<uint32>(b) << 16) | (static_cast<uint32>(g) << 8) |
                    (static_cast<uint32>(r)));
    return colour;
}

FX_FORCE_INLINE FxColor FxColor::FromFloats(float32 rgba[4])
{
    FxColor colour;
    colour.Value = ((static_cast<uint32>(rgba[3] * 255.0) << 24) | (static_cast<uint32>(rgba[2] * 255.0) << 16) |
                    (static_cast<uint32>(rgba[1] * 255.0) << 8) | (static_cast<uint32>(rgba[0] * 255.0)));
    return colour;
}
