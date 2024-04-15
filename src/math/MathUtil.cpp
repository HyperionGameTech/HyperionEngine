/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */
#include "MathUtil.hpp"

namespace hyperion {

uint64 MathUtil::g_seed = ~0u;

Vec3f MathUtil::RandomInSphere(Vec3f rnd)
{
    float ang1 = (rnd.x + 1.0) * pi<float>;
    float u = rnd.y;
    float u2 = u * u;
    float sqrt1MinusU2 = Sqrt(1.0 - u2);
    float x = sqrt1MinusU2 * Cos(ang1);
    float y = sqrt1MinusU2 * Sin(ang1);
    float z = u;

    return { x, y, z };
}

Vec3f MathUtil::RandomInHemisphere(Vec3f rnd, Vec3f n)
{
    const Vec3f v = RandomInSphere(rnd);

    return v * float(Sign(v.Dot(n)));
}

Vec2f MathUtil::VogelDisk(uint sample_index, uint num_samples, float phi)
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
 //   // http://www.blackpawn.com/texts/pointinpoly/
	//// Compute vectors
	//Vec2f v0 = p3 - p1;
	//Vec2f v1 = p2 - p1;
	//Vec2f v2 = p - p1;
	//// Compute dot products
	//float dot00 = v0.Dot(v0);
	//float dot01 = v0.Dot(v1);
	//float dot02 = v0.Dot(v2);
	//float dot11 = v1.Dot(v1);
	//float dot12 = v1.Dot(v2);
	//// Compute barycentric coordinates
	//float invDenom = 1.0f / (dot00 * dot11 - dot01 * dot01);
	//float u = (dot11 * dot02 - dot01 * dot12) * invDenom;
	//float v = (dot00 * dot12 - dot01 * dot02) * invDenom;
	//return Vec2f(u, v);

    Vec3f s[2];
    for (int i=2; i--; ) {
        s[i][0] = v2[i]-v0[i];
        s[i][1] = v1[i]-v0[i];
        s[i][2] = v0[i]-p[i];
    }
    Vec3f u = s[0].Cross(s[1]);
    if (Abs(u[2])>1e-2) // dont forget that u[2] is integer. If it is zero then triangle ABC is degenerate
        return Vec3f(1.f-(u.x+u.y)/u.z, u.y/u.z, u.x/u.z);
    return Vec3f(-1,1,1); // in this case generate negative coordinates, it will be thrown away by the rasterizator
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
