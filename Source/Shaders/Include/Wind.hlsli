#ifndef HYP_WIND
#define HYP_WIND

#include "Scene.hlsli"

#define WIND_TAU 6.28318531

struct WindVertex
{
    // rest position in mesh space; a tree's foot sits at the origin
    float3 local_position;
    // per sway order (VT_Tree): the pivot it bends about xyz, how far a point here swings w
    float4 limb;
    float4 branch;
    float4 twig;
    // leaf cards (VT_Foliage): the twig point they hang from xyz, how much they flutter w
    float4 foliage;
};

// Four halves packed into two uints, the way VT_Tree and VT_Foliage store them
float4 UnpackWindHalves(uint2 packed)
{
    return float4(f16tof32(packed.x), f16tof32(packed.x >> 16), f16tof32(packed.y), f16tof32(packed.y >> 16));
}

float WindHash(float3 p)
{
    return frac(sin(dot(p, float3(12.9898, 78.233, 37.719))) * 43758.5453);
}

// Deflection along a cantilever under an even load: 0 at the fixed end, 1 at the free one
float WindCantilever(float x)
{
    x = saturate(x);

    return x * x * (6.0 - 4.0 * x + x * x) / 3.0;
}

float3 WindDown(WorldShaderData world)
{
    return float3(world.wind_params.x, 0.0, world.wind_params.y);
}

float3 WindAcross(WorldShaderData world)
{
    return float3(-world.wind_params.y, 0.0, world.wind_params.x);
}

float WindGust(WorldShaderData world, float3 p, float time)
{
    const float t = time - dot(p.xz, world.wind_params.xy) / 8.0;
    const float g = 0.5 + 0.26 * sin(t * 0.53) + 0.16 * sin(t * 1.31 + 1.7) + 0.08 * sin(t * 3.1 + 0.4);

    return world.wind_params.z * max(1.0 + world.wind_params.w * (2.0 * g - 1.0), 0.0);
}

float3 WindBend(float3 p, float3 pivot, float3 push)
{
    const float3 arm = p - pivot;
    const float armLength = length(arm);

    if (armLength < 1e-4)
    {
        return p;
    }

    return pivot + normalize(arm + push) * armLength;
}

float3 WindRotate(float3 v, float3 axis, float angle)
{
    const float c = cos(angle);
    const float s = sin(angle);

    return v * c + cross(axis, v) * s + axis * dot(axis, v) * (1.0 - c);
}

float3 WindOrder(WorldShaderData world, float3 p, float3 pivot, float weight, float frequency, float pace, float time)
{
    if (weight <= 0.0)
    {
        return p;
    }

    const float h = WindHash(pivot);
    const float phase = h * WIND_TAU;
    const float omega = WIND_TAU * frequency * pace * (0.8 + 0.4 * frac(h * 7.31));

    const float3 push = WindDown(world) * (0.55 + 0.45 * sin(omega * time + phase))
        + float3(0.0, 0.6 * sin(omega * 1.37 * time + phase * 1.9), 0.0)
        + WindAcross(world) * (0.35 * sin(omega * 0.73 * time + phase * 2.7));

    return WindBend(p, pivot, push * (WindGust(world, pivot, time) * weight));
}

float3 WindTrunk(WorldShaderData world, float3 p, float3 treeOrigin, float weight, float frequency, float heightAboveFoot, float time)
{
    if (weight <= 0.0)
    {
        return p;
    }

    const float omega = WIND_TAU * frequency;
    const float phase = WindHash(treeOrigin) * WIND_TAU;
    const float sway = world.wind_params.z * (0.2 + 0.4 * world.wind_params.w);

    const float3 push = WindDown(world) * (WindGust(world, treeOrigin, time) + sway * sin(omega * time + phase))
        + WindAcross(world) * (sway * 0.35 * sin(omega * 0.81 * time + phase + 1.1));

    return WindBend(p, p - float3(0.0, heightAboveFoot, 0.0), push * weight);
}

// How far the wind moves a point at rest at `time`; also turns a leaf card's normal as it flutters.
// tree_wind is the material's: sway frequency, trunk flexibility, tree height, leaf flutter
float3 WindDisplacement(WorldShaderData world, float4x4 model, float4 tree_wind, WindVertex wind, float3 p, inout float3 normal, float time, bool withFlutter)
{
    if (world.wind_params.z <= 0.0)
    {
        return float3(0.0, 0.0, 0.0);
    }

    const float scale = length(float3(model[0][0], model[1][0], model[2][0]));
    const float3 treeOrigin = float3(model[0][3], model[1][3], model[2][3]);
    const float frequency = tree_wind.x;

#ifdef VT_Foliage
    // a leaf card rides the twig point it hangs from rigidly, and flutters about it
    const float3 origin = mul(model, float4(wind.foliage.xyz, 1.0)).xyz;
    const float heightAboveFoot = max(wind.foliage.y, 0.0);
#else // !VT_Foliage
    const float3 origin = p;
    const float heightAboveFoot = max(wind.local_position.y, 0.0);
#endif // VT_Foliage

    float3 local = p - origin;

#ifdef VT_Foliage
    const float flutter = tree_wind.w * wind.foliage.w;

    if (withFlutter && flutter > 0.0)
    {
        const float h = WindHash(origin);
        const float omega = WIND_TAU * (3.5 + 3.0 * h);
        const float angle = flutter * WindGust(world, origin, time)
            * (0.7 * sin(omega * time + h * WIND_TAU) + 0.3 * sin(omega * 2.3 * time + h * 17.0));

        const float a = frac(h * 13.7) * WIND_TAU;
        const float3 axis = normalize(WindAcross(world) * cos(a) + WindDown(world) * sin(a) + float3(0.0, 0.35 * (frac(h * 5.3) - 0.5), 0.0));

        local = WindRotate(local, axis, angle);
        normal = WindRotate(normal, axis, angle);
    }
#endif // VT_Foliage

    float3 q = origin;

#ifdef VT_Tree
    q = WindOrder(world, q, mul(model, float4(wind.twig.xyz, 1.0)).xyz, wind.twig.w * scale, frequency, 4.6, time);
    q = WindOrder(world, q, mul(model, float4(wind.branch.xyz, 1.0)).xyz, wind.branch.w * scale, frequency, 3.0, time);
    q = WindOrder(world, q, mul(model, float4(wind.limb.xyz, 1.0)).xyz, wind.limb.w * scale, frequency, 1.9, time);
#endif // VT_Tree

    const float treeHeight = tree_wind.z;

    if (treeHeight > 0.0)
    {
        const float trunkWeight = tree_wind.y * treeHeight * WindCantilever(heightAboveFoot / treeHeight);

        q = WindTrunk(world, q, treeOrigin, trunkWeight * scale, frequency, heightAboveFoot * scale, time);
    }

    return q + local - p;
}

#endif // HYP_WIND
