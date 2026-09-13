/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <Core/Defines.hpp>

#include <Core/Math/MathUtil.hpp>
#include <Core/Math/Vector2.hpp>

#include <Core/Types.hpp>

namespace Hyperion {

HYP_FORCE_INLINE uint32 TerrainHashU32(uint32 value)
{
    value ^= value >> 16;
    value *= 0x7FEB352Du;
    value ^= value >> 15;
    value *= 0x846CA68Bu;
    value ^= value >> 16;

    return value;
}

HYP_FORCE_INLINE uint32 TerrainHashCoord(uint32 seed, int32 x, int32 y)
{
    uint32 hash = seed * 0x9E3779B9u;
    hash = TerrainHashU32(hash + uint32(x) * 0x85EBCA6Bu);
    hash = TerrainHashU32(hash + uint32(y) * 0xC2B2AE35u);

    return hash;
}

HYP_FORCE_INLINE uint32 TerrainHashOctaveSeed(uint32 seed, uint32 octave, uint32 salt = 0u)
{
    return TerrainHashU32(seed + (octave + 1u) * 0x9E3779B9u + salt * 0x85EBCA77u);
}

HYP_FORCE_INLINE float TerrainSmoothStep(float edge0, float edge1, float value)
{
    const float t = MathUtil::Clamp((value - edge0) / MathUtil::Max(edge1 - edge0, 1e-6f), 0.0f, 1.0f);

    return t * t * (3.0f - 2.0f * t);
}

float TerrainSimplex2D(uint32 seed, float x, float y);
float TerrainFbm2D(uint32 seed, float x, float y, uint32 octaves, float lacunarity = 2.0f, float gain = 0.5f);
float TerrainRidged2D(uint32 seed, float x, float y, uint32 octaves, float lacunarity = 2.02f, float gain = 0.5f);

Vec2f TerrainDomainWarp(uint32 seed, const Vec2f& xz, float frequency, float amplitude);

} // namespace Hyperion
