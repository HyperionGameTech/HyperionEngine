/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <core/math/MathUtil.hpp>

#include <core/containers/Array.hpp>

namespace hyperion {

uint64 MathUtil::g_seed = ~0u;

static Array<Vec2f> FindFactors(int num)
{
    Array<Vec2f> factors;

    for (int i = 1; i <= num; i++) {
        if (num % i == 0) {
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

Vec2i MathUtil::ReshapeExtent(Vec2i extent)
{
    Array<Vec2f> factors = FindFactors(extent.x * extent.y);

    if (factors.Size() == 0) {
        return Vec2i::Zero();
    }

    // Sort it so lowest difference is front
    std::sort(
        factors.Begin(),
        factors.End(),
        [](const Vec2f &a, const Vec2f &b)
        {
            const float a_diff = MathUtil::Abs(a[0] - a[1]);
            const float b_diff = MathUtil::Abs(b[0] - b[1]);

            return a_diff < b_diff;
        }
    );

    const Vec2f most_balanced_pair = factors[0];

    return Vec2i(most_balanced_pair);
}

Vec2f MathUtil::Hammersley(uint32 sample_index, uint32 num_samples)
{
    return { float(sample_index) / float(num_samples), VanDerCorpus(sample_index) };
}

Vec3f MathUtil::RandomInSphere(Vec3f rnd)
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

Vec3f MathUtil::RandomInHemisphere(Vec3f rnd, Vec3f n)
{
    const Vec3f v = RandomInSphere(rnd);

    return v * float(Sign(v.Dot(n.Normalize())));
}

Vec2f MathUtil::VogelDisk(uint32 sample_index, uint32 num_samples, float phi)
{
    constexpr float golden_angle = 2.4f;

    float r = Sqrt(float(sample_index) + 0.5f) / Sqrt(float(num_samples));
    float theta = sample_index * golden_angle + phi;

    return { r * Cos(theta), r * Sin(theta) };
}

Vec3f MathUtil::ImportanceSampleGGX(Vec2f Xi, Vec3f N, float roughness)
{
    float alpha = roughness * roughness;
    float alpha2 = alpha * alpha;
	
    float phi = 2.0f * pi<float> * Xi.x;
    float cos_theta = Sqrt((1.0f - Xi.y) / (1.0f + (alpha2 - 1.0f) * Xi.y));
    float sin_theta = Sqrt(1.0f - cos_theta * cos_theta);
	
    // from spherical coordinates to cartesian coordinates
    return { Cos(phi) * sin_theta, Sin(phi) * sin_theta, cos_theta };
}

Vec3f MathUtil::CalculateBarycentricCoordinates(const Vec3f &v0, const Vec3f &v1, const Vec3f &v2, const Vec3f &p)
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

Vec3f MathUtil::CalculateBarycentricCoordinates(const Vec2f &v0, const Vec2f &v1, const Vec2f &v2, const Vec2f &p)
{
    Vec3f s[2];

    for (int i = 2; i--;) {
        s[i][0] = v2[i] - v0[i];
        s[i][1] = v1[i] - v0[i];
        s[i][2] = v0[i] - p[i];
    }

    Vec3f u = s[0].Cross(s[1]);

    if (Abs(u.z) > 1e-2) {
        return Vec3f(1.0f - (u.x + u.y) / u.z, u.y / u.z, u.x / u.z);
    }

    return Vec3f(-1, 1, 1);
}

void MathUtil::ComputeOrthonormalBasis(const Vec3f &normal, Vec3f &out_tangent, Vec3f &out_bitangent)
{
    Vec3f T;
    T = normal.Cross(Vec3f::UnitY());
    T = Lerp(normal.Cross(Vec3f::UnitX()), Vec3f::UnitX(), Step(epsilon_f, T.Dot(T)));
    T.Normalize();

    out_tangent = T;
    out_bitangent = normal.Cross(T).Normalize();
}

} // namespace hyperion
