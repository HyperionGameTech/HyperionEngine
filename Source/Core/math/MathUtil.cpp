/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <core/math/MathUtil.hpp>

#include <core/containers/Array.hpp>

namespace Hyperion {

namespace MathUtil {

static Array<Vec2f> FindFactors(int num)
{
    Array<Vec2f> factors;

    for (int i = 1; i <= num; i++)
    {
        if (num % i == 0)
        {
            factors.PushBack(Vec2f { float(i), float(num) / float(i) });
        }
    }

    return factors;
}

float VanDerCorpus(uint32 bits)
{
    bits = (bits << 16u) | (bits >> 16u);
    bits = ((bits & 0x55555555u) << 1u) | ((bits & 0xAAAAAAAAu) >> 1u);
    bits = ((bits & 0x33333333u) << 2u) | ((bits & 0xCCCCCCCCu) >> 2u);
    bits = ((bits & 0x0F0F0F0Fu) << 4u) | ((bits & 0xF0F0F0F0u) >> 4u);
    bits = ((bits & 0x00FF00FFu) << 8u) | ((bits & 0xFF00FF00u) >> 8u);
    return float(bits) * 2.3283064365386963e-10; // / 0x100000000
}

Vec2i ReshapeExtent(Vec2i extent)
{
    Array<Vec2f> factors = FindFactors(extent.x * extent.y);

    if (factors.Size() == 0)
    {
        return Vec2i::Zero();
    }

    // Sort it so lowest difference is front
    std::sort(
        factors.Begin(),
        factors.End(),
        [](const Vec2f& a, const Vec2f& b)
        {
            const float aDiff = Abs(a[0] - a[1]);
            const float bDiff = Abs(b[0] - b[1]);

            return aDiff < bDiff;
        });

    const Vec2f mostBalancedPair = factors[0];

    return Vec2i(mostBalancedPair);
}

Vec2f Hammersley(uint32 sampleIndex, uint32 numSamples)
{
    return { float(sampleIndex) / float(numSamples), VanDerCorpus(sampleIndex) };
}

Vec3f RandomInSphere(Vec3f rnd)
{
    float ang1 = (rnd.x + 1.0) * pi<float>;
    float u = rnd.y;
    float u2 = u * u;
    float sqrt1MinusU2 = Sqrt(1.0f - u2);
    float x = sqrt1MinusU2 * Cos(ang1);
    float y = sqrt1MinusU2 * Sin(ang1);
    float z = u;

    return { x, y, z };
}

Vec3f RandomInHemisphere(Vec3f rnd, Vec3f n)
{
    const Vec3f v = RandomInSphere(rnd);

    return v * float(Sign(v.Dot(n.Normalize())));
}

Vec2f VogelDisk(uint32 sampleIndex, uint32 numSamples, float phi)
{
    constexpr float goldenAngle = 2.4f;

    float r = Sqrt(float(sampleIndex) + 0.5f) / Sqrt(float(numSamples));
    float theta = sampleIndex * goldenAngle + phi;

    return { r * Cos(theta), r * Sin(theta) };
}

Vec3f ImportanceSampleGGX(Vec2f xi, Vec3f n, float roughness)
{
    float alpha = roughness * roughness;
    float alpha2 = alpha * alpha;

    float phi = 2.0f * pi<float> * xi.x;
    float cosTheta = Sqrt((1.0f - xi.y) / (1.0f + (alpha2 - 1.0f) * xi.y));
    float sinTheta = Sqrt(1.0f - cosTheta * cosTheta);

    // from spherical coordinates to cartesian coordinates
    return { Cos(phi) * sinTheta, Sin(phi) * sinTheta, cosTheta };
}

Vec3f CalculateBarycentricCoordinates(const Vec3f& v0, const Vec3f& v1, const Vec3f& v2, const Vec3f& p)
{
    const Vec3f e0 = v1 - v0;
    const Vec3f e1 = v2 - v0;
    const Vec3f e2 = p - v0;

    const float d00 = e0.Dot(e0);
    const float d01 = e0.Dot(e1);
    const float d11 = e1.Dot(e1);
    const float d20 = e2.Dot(e0);
    const float d21 = e2.Dot(e1);

    const float denom = d00 * d11 - d01 * d01;

    const float v = (d11 * d20 - d01 * d21) / denom;
    const float w = (d00 * d21 - d01 * d20) / denom;
    const float u = 1.0f - v - w;

    return { u, v, w };
}

Vec3f CalculateBarycentricCoordinates(const Vec2f& v0, const Vec2f& v1, const Vec2f& v2, const Vec2f& p)
{
    Vec3f s[2];

    for (int i = 2; i--;)
    {
        s[i][0] = v2[i] - v0[i];
        s[i][1] = v1[i] - v0[i];
        s[i][2] = v0[i] - p[i];
    }

    Vec3f u = s[0].Cross(s[1]);

    if (Abs(u.z) > 1e-2)
    {
        return Vec3f(1.0f - (u.x + u.y) / u.z, u.y / u.z, u.x / u.z);
    }

    return Vec3f(-1, 1, 1);
}

void ComputeOrthonormalBasis(const Vec3f& normal, Vec3f& outTangent, Vec3f& outBitangent)
{
    Vec3f t;
    t = normal.Cross(Vec3f::UnitY());
    t = Lerp(normal.Cross(Vec3f::UnitX()), Vec3f::UnitX(), Step(epsilonF, t.Dot(t)));
    t.Normalize();

    outTangent = t;
    outBitangent = normal.Cross(t).Normalize();
}

Vec2f EncodeOctahedralCoord(const Vec3f& in)
{
#define NON_ZERO_SIGN(n) (n >= 0.0f ? 1.0f : -1.0f)

    float l1norm = Abs(in.x) + Abs(in.y) + Abs(in.z);
    Vec2f vec = in.GetXY() * (1.0f / l1norm);

    if (in.z < 0.0f)
    {
        vec = (Vec2f::One() - Abs(Vec2f(vec.y, vec.x))) * Vec2f(NON_ZERO_SIGN(vec.x), NON_ZERO_SIGN(vec.y));
    }

#undef NON_ZERO_SIGN

    return vec;
}

Vec3f DecodeOctahedralCoord(const Vec2f& in)
{
#define NON_ZERO_SIGN(n) (n >= 0.0 ? 1.0 : -1.0)

    Vec3f vec = Vec3f(in.x, in.y, 1.0 - Abs(in.x) - Abs(in.y));

    if (vec.z < 0.0f)
    {
        const Vec2f xy = (Vec2f::One() - Abs(vec.GetXY())) * Vec2f(NON_ZERO_SIGN(vec.x), NON_ZERO_SIGN(vec.y));
        vec.x = xy.x;
        vec.y = xy.y;
    }

#undef NON_ZERO_SIGN

    return vec.Normalize();
}

Vec2f NormalizeOctahedralCoord(const Vec2i& coord, const Vec2i& extent)
{
    return (Vec2f(coord) + Vec2f(0.5f)) * (Vec2f(2.0f) / Vec2f(extent)) - Vec2f::One();
}

} // namespace MathUtil

} // namespace Hyperion
