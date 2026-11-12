#ifndef GLIMMER_SURFEL_TYPES_HLSLI
#define GLIMMER_SURFEL_TYPES_HLSLI

#include "../GlimmerCommon.hlsli"
#include "../GlimmerLight.hlsli"

// NOTE must match GlimmerSurfelCache.hpp
#define GLIMMER_SURFEL_CAPACITY (1u << 18)
#define GLIMMER_SURFEL_HASH_ENTRIES (1u << 19)
#define GLIMMER_SURFEL_RAYS 32u

#define GLIMMER_SURFEL_NONE 0xFFFFFFFFu
#define GLIMMER_SURFEL_HASH_EMPTY 0u
#define GLIMMER_SURFEL_HASH_PROBES 32u

#define GLIMMER_SURFEL_COUNTER_SURFELS 0u
#define GLIMMER_SURFEL_COUNTER_DROPPED 1u
#define GLIMMER_SURFEL_COUNTER_FOLIAGE 2u
#define GLIMMER_SURFEL_COUNTER_FOLIAGE_DROPPED 3u
#define GLIMMER_SURFEL_COUNTERS 4u

#define GLIMMER_FOLIAGE_CAPACITY (1u << 16)
#define GLIMMER_FOLIAGE_HASH_ENTRIES (1u << 17)

// the foliage buffer: 4 per hash entry (the voxel's key, its packed cell, then its leaf area until it is allocated and its index after),
// then GLIMMER_FOLIAGE_UINTS per voxel
#define GLIMMER_FOLIAGE_HASH_UINTS (GLIMMER_FOLIAGE_HASH_ENTRIES * 4u)

#define GLIMMER_FOLIAGE_CELL 0u     // 3 ints
#define GLIMMER_FOLIAGE_STATE 3u    // updates
#define GLIMMER_FOLIAGE_VISIBLE 4u  // how much of it can be seen (float bits)
#define GLIMMER_FOLIAGE_COMPOSED 5u // L1 per colour channel under the current light, as halves
#define GLIMMER_FOLIAGE_LIGHT 11u   // GLIMMER_LIGHT_ENTRIES as halves
#define GLIMMER_FOLIAGE_SUN 67u     // the sun bounce, L1 per colour channel as halves, then its sun state (GLIMMER_SUN_BOUNCE_UINTS)
#define GLIMMER_FOLIAGE_SUN_KEYS 74u      // the sun bounce at each key along the sun's arc (GLIMMER_SUN_KEY_L1_UINTS each)
#define GLIMMER_FOLIAGE_SUN_KEY_STATE 102u // the arc's epoch and the updates the keys hold
#define GLIMMER_FOLIAGE_UINTS 103u

// a sun bounce held as L1 per colour channel: 2 uints a channel, then the sun epoch and updates it holds (GlimmerSurfelSunFreshness)
#define GLIMMER_SUN_BOUNCE_UINTS 7u
#define GLIMMER_SUN_BOUNCE_STATE 6u

#define GLIMMER_FOLIAGE_AREA_SCALE 256.0

// the parts as half3s (2 uints each), then the sky's direct terms as halves
#define GLIMMER_SURFEL_DIRECT_UINT (GLIMMER_LIGHT_PARTS * 2u)
#define GLIMMER_SURFEL_LIGHT_UINTS (GLIMMER_SURFEL_DIRECT_UINT + (GLIMMER_LIGHT_TERMS + 1u) / 2u)

// the hash buffer: the counters, then 2 per entry: the cell's key, then its first surfel
#define GLIMMER_SURFEL_HASH_UINTS (GLIMMER_SURFEL_COUNTERS + GLIMMER_SURFEL_HASH_ENTRIES * 2u)

// places along the sun's arc a surfel keeps its sun bounce for (NOTE must match GlimmerSunKeys in GlimmerSurfelCache.hpp)
#ifndef GLIMMER_SUN_KEYS
#define GLIMMER_SUN_KEYS 14u
#endif
#define GLIMMER_SUN_KEY_NONE 0xFFFFFFFFu

// the keys, then the sun now
#define GLIMMER_SURFEL_SUN_SLOTS (GLIMMER_SUN_KEYS + 1u)
#define GLIMMER_SURFEL_SUN_VISIBILITY_UINTS ((GLIMMER_SURFEL_SUN_SLOTS + 3u) / 4u)
#define GLIMMER_SUN_KEY_ALL 0xFFFFFFFEu

// a key's sun bounce as L1 per colour channel: the colour (RGB9E5), then the direction all channels share, relative to it (10 bits an axis)
#define GLIMMER_SUN_KEY_L1_UINTS 2u

// the data buffer, per surfel: the next one in its cell, its irradiance under the current light (GlimmerPackSurfelComposed), then its light,
// then its sun bounce as a half3 and the sun epoch and updates it holds (GlimmerSurfelSunFreshness),
// then its sun bounce at each key along the sun's arc (RGB9E5) and the arc's epoch and the updates they hold,
// then the same keys averaged with the surfels around it, which is what its light is composed with,
// then how much of the sun reaches it at each key and now (a byte each), which hits on it take in place of a shadow ray of their own
#define GLIMMER_SURFEL_DATA_NEXT 0u
#define GLIMMER_SURFEL_DATA_COMPOSED 1u
#define GLIMMER_SURFEL_DATA_LIGHT 3u
#define GLIMMER_SURFEL_DATA_SUN (GLIMMER_SURFEL_DATA_LIGHT + GLIMMER_SURFEL_LIGHT_UINTS)
#define GLIMMER_SURFEL_DATA_SUN_STATE (GLIMMER_SURFEL_DATA_SUN + 2u)
#define GLIMMER_SURFEL_DATA_SUN_KEYS (GLIMMER_SURFEL_DATA_SUN_STATE + 1u)
#define GLIMMER_SURFEL_DATA_SUN_KEY_STATE (GLIMMER_SURFEL_DATA_SUN_KEYS + GLIMMER_SUN_KEYS)
#define GLIMMER_SURFEL_DATA_SUN_KEYS_SMOOTHED (GLIMMER_SURFEL_DATA_SUN_KEY_STATE + 1u)
#define GLIMMER_SURFEL_DATA_SUN_VISIBILITY (GLIMMER_SURFEL_DATA_SUN_KEYS_SMOOTHED + GLIMMER_SUN_KEYS)
#define GLIMMER_SURFEL_DATA_UINTS (GLIMMER_SURFEL_DATA_SUN_VISIBILITY + GLIMMER_SURFEL_SUN_VISIBILITY_UINTS)

uint GlimmerSurfelHashKeyIndex(uint slot)
{
    return GLIMMER_SURFEL_COUNTERS + slot * 2u;
}

uint GlimmerSurfelDataIndex(uint surfelIndex, uint field)
{
    return surfelIndex * GLIMMER_SURFEL_DATA_UINTS + field;
}

#define GLIMMER_SURFEL_ORIGIN_BIAS 0.03

// a ray's GLIMMER_RAY_UINTS, its sun bounce as a half3, then room for its hit until the shade pass,
// then its sun bounce for every key along the sun's arc (RGB9E5), then the surfel's sun visibility in the slot the ray's index is
// (NOTE must match RayRecordSize in GlimmerSurfelCache.cpp)
#define GLIMMER_SURFEL_RAY_SUN_UINT GLIMMER_RAY_UINTS
#define GLIMMER_SURFEL_RAY_SUN_KEYS_UINT (GLIMMER_RAY_UINTS + 12u)
#define GLIMMER_SURFEL_RAY_SUN_VISIBILITY_UINT (GLIMMER_SURFEL_RAY_SUN_KEYS_UINT + GLIMMER_SUN_KEYS)
#define GLIMMER_SURFEL_RAY_RECORD_UINTS (GLIMMER_SURFEL_RAY_SUN_VISIBILITY_UINT + 1u)

// updates a surfel's sun bounce takes to replace the SH sun in full
#define GLIMMER_SURFEL_SUN_SETTLE 4.0

// updates the sun bounce averages at most, so it follows a turning sun and forgets the first updates' missing bounces
#define GLIMMER_SURFEL_SUN_HISTORY 16u

// keys a hit tests the sun's visibility for when a trace is for every key: an even spread, each standing in for GLIMMER_SUN_KEYS / this of them
#define GLIMMER_SURFEL_SUN_KEY_SHADOWS 2u
#define GLIMMER_SURFEL_SUN_KEY_STRIDE (GLIMMER_SUN_KEYS / GLIMMER_SURFEL_SUN_KEY_SHADOWS)

// updates each key's sun bounce takes to replace the SH sun in full, and that it averages at most
#define GLIMMER_SURFEL_SUN_KEY_SETTLE 4u
#define GLIMMER_SURFEL_SUN_KEY_HISTORY 64u

struct GlimmerSurfels
{
    uint4 info;     // x = surfels and foliage voxels traced this frame at most, y = first of them traced this frame, z = frame, w = 1 when they can be sampled
    float4 params;  // x = hash cell size, y = 1 / cell size, z = surfel radius, w = how far rays trace the near field (m)
    float4 foliage; // x = foliage voxel size, y = 1 / voxel size, z = the sun epoch the sun last jumped at, w = 1 when the sun bounce is traced
    float4 sun;     // xyz = toward the sun the sun bounce is traced for, w = that direction's epoch
    float4 sunArc;  // x = where the sun is now along the keys (< 0 off the arc), y = 1 when surfels keep their sun bounce at the keys, z = the arc's epoch
    float4 sunKeys[GLIMMER_SUN_KEYS]; // xyz = toward the sun at each key
};

// the key a trace is for, GLIMMER_SUN_KEY_ALL when it is for every key at once, GLIMMER_SUN_KEY_NONE for the sun now:
// what its hits receive of the sun is for the same key
static uint glimmerSunKey = GLIMMER_SUN_KEY_NONE;

bool GlimmerSurfelHasSunKeys(GlimmerSurfels surfels)
{
    return surfels.sunArc.y != 0.0;
}

// the updates a surfel's keys hold, none from before the arc last changed
uint GlimmerSurfelSunKeyUpdates(GlimmerSurfels surfels, uint keyState)
{
    return (keyState >> 16) == (uint(surfels.sunArc.z) & 0xFFFFu) ? (keyState & 0xFFFFu) : 0u;
}

uint GlimmerSurfelNextSunKeyState(GlimmerSurfels surfels, uint keyUpdates)
{
    return ((uint(surfels.sunArc.z) & 0xFFFFu) << 16) | min(keyUpdates + 1u, 0xFFFFu);
}

// surfels and foliage voxels trace every key at once and settle after GLIMMER_SURFEL_SUN_KEY_SETTLE updates;
// the SH grid traces them in turn and stops after a couple of passes a key, so it settles once each has been traced
float GlimmerSurfelSunKeyFreshness(uint keyUpdates, uint settleUpdates)
{
    return saturate(float(keyUpdates) / float(settleUpdates));
}

float GlimmerSurfelSunKeyBlendWeight(uint keyUpdates, uint updatesPerKey)
{
    return 1.0 / (float(min(keyUpdates / updatesPerKey, GLIMMER_SURFEL_SUN_KEY_HISTORY)) + 1.0);
}

bool GlimmerSunKeyIsOne()
{
    return glimmerSunKey < GLIMMER_SUN_KEYS;
}

uint GlimmerSunKeyOf(GlimmerSurfels surfels, uint keyState)
{
    return GlimmerSurfelSunKeyUpdates(surfels, keyState) % GLIMMER_SUN_KEYS;
}

float3 GlimmerSurfelSunDirection(GlimmerSurfels surfels)
{
    return GlimmerSunKeyIsOne() ? surfels.sunKeys[glimmerSunKey].xyz : surfels.sun.xyz;
}

uint GlimmerPackRGB9E5(float3 color)
{
    const float3 clamped = clamp(color, 0.0, 65408.0);
    const float maxChannel = max(clamped.r, max(clamped.g, clamped.b));

    int exponent = max(-16, int(floor(log2(max(maxChannel, 1e-30))))) + 16;
    float scale = exp2(float(exponent - 24));

    if (floor(maxChannel / scale + 0.5) >= 512.0)
    {
        exponent += 1;
        scale *= 2.0;
    }

    const uint3 mantissa = uint3(min(floor(clamped / scale + 0.5), 511.0));

    return (uint(exponent) << 27) | (mantissa.b << 18) | (mantissa.g << 9) | mantissa.r;
}

float3 GlimmerUnpackRGB9E5(uint packed)
{
    const float scale = exp2(float(int(packed >> 27) - 24));

    return float3(float(packed & 0x1FFu), float((packed >> 9) & 0x1FFu), float((packed >> 18) & 0x1FFu)) * scale;
}

uint2 GlimmerPackSunKeyL1(float4 sun[3])
{
    const float3 mean = float3(sun[0].x, sun[1].x, sun[2].x);
    const float meanLuminance = dot(max(mean, (float3)0.0), float3(0.2126, 0.7152, 0.0722));
    const float3 gradient = sun[0].yzw * 0.2126 + sun[1].yzw * 0.7152 + sun[2].yzw * 0.0722;

    const float3 relative = meanLuminance > 1e-8 ? clamp(gradient / meanLuminance, -2.0, 2.0) : (float3)0.0;
    const uint3 quantized = uint3(round((relative * 0.25 + 0.5) * 1023.0));

    return uint2(GlimmerPackRGB9E5(mean), quantized.x | (quantized.y << 10) | (quantized.z << 20));
}

void GlimmerUnpackSunKeyL1(uint2 packed, out float4 outSun[3])
{
    const float3 mean = GlimmerUnpackRGB9E5(packed.x);
    const float3 relative = (float3(packed.y & 0x3FFu, (packed.y >> 10) & 0x3FFu, (packed.y >> 20) & 0x3FFu) / 1023.0 - 0.5) * 4.0;

    [unroll]
    for (uint channel = 0; channel < 3; channel++)
    {
        outSun[channel] = float4(mean[channel], relative * mean[channel]);
    }
}

// a sun bounce held as L1 per colour channel for the key a trace is for, or for the sun now between the two keys either side of it,
// and how fresh it is; outHasKeys is false when there are no keys to give (no arc, or the sun off it)
#define GLIMMER_KEYED_SUN_L1(buffer, surfels, keysIndex, keyStateIndex, settleUpdates, outSun, outFreshness, outHasKeys) \
    { \
        outFreshness = 0.0; \
        outHasKeys = false; \
        [unroll] for (uint keyChannel_ = 0; keyChannel_ < 3; keyChannel_++) { outSun[keyChannel_] = (float4)0.0; } \
        if (GlimmerSurfelHasSunKeys(surfels) && (GlimmerSunKeyIsOne() || (surfels).sunArc.x >= 0.0)) \
        { \
            outHasKeys = true; \
            outFreshness = GlimmerSurfelSunKeyFreshness(GlimmerSurfelSunKeyUpdates(surfels, buffer[keyStateIndex]), settleUpdates); \
            if (GlimmerSunKeyIsOne()) \
            { \
                const uint keyAt_ = (keysIndex) + glimmerSunKey * GLIMMER_SUN_KEY_L1_UINTS; \
                GlimmerUnpackSunKeyL1(uint2(buffer[keyAt_], buffer[keyAt_ + 1u]), outSun); \
            } \
            else \
            { \
                const float keyPlace_ = min((surfels).sunArc.x, float(GLIMMER_SUN_KEYS - 1u)); \
                const uint keyBefore_ = min(uint(keyPlace_), GLIMMER_SUN_KEYS - 2u); \
                const uint keyAt_ = (keysIndex) + keyBefore_ * GLIMMER_SUN_KEY_L1_UINTS; \
                float4 keyAfterSun_[3]; \
                GlimmerUnpackSunKeyL1(uint2(buffer[keyAt_], buffer[keyAt_ + 1u]), outSun); \
                GlimmerUnpackSunKeyL1(uint2(buffer[keyAt_ + 2u], buffer[keyAt_ + 3u]), keyAfterSun_); \
                [unroll] for (uint keyChannel_ = 0; keyChannel_ < 3; keyChannel_++) { outSun[keyChannel_] = lerp(outSun[keyChannel_], keyAfterSun_[keyChannel_], keyPlace_ - float(keyBefore_)); } \
            } \
        } \
    }

// a surfel's sun bounce for the key its trace is for, or for the sun now between the two keys either side of it, and how fresh it is;
// outHasKeys is false when there are no keys to give (no arc, or the sun off it)
#define GLIMMER_SURFEL_KEYED_SUN(buffer, surfels, surfelIndex, keysField, outSun, outFreshness, outHasKeys) \
    { \
        outSun = (float3)0.0; \
        outFreshness = 0.0; \
        outHasKeys = false; \
        if (GlimmerSurfelHasSunKeys(surfels) && (GlimmerSunKeyIsOne() || (surfels).sunArc.x >= 0.0)) \
        { \
            const uint keyBase_ = GlimmerSurfelDataIndex(surfelIndex, keysField); \
            const uint keyUpdates_ = GlimmerSurfelSunKeyUpdates(surfels, buffer[GlimmerSurfelDataIndex(surfelIndex, GLIMMER_SURFEL_DATA_SUN_KEY_STATE)]); \
            outHasKeys = true; \
            outFreshness = GlimmerSurfelSunKeyFreshness(keyUpdates_, GLIMMER_SURFEL_SUN_KEY_SETTLE); \
            if (GlimmerSunKeyIsOne()) \
            { \
                outSun = GlimmerUnpackRGB9E5(buffer[keyBase_ + glimmerSunKey]); \
            } \
            else \
            { \
                const float keyAt_ = min((surfels).sunArc.x, float(GLIMMER_SUN_KEYS - 1u)); \
                const uint keyBefore_ = min(uint(keyAt_), GLIMMER_SUN_KEYS - 2u); \
                outSun = lerp(GlimmerUnpackRGB9E5(buffer[keyBase_ + keyBefore_]), GlimmerUnpackRGB9E5(buffer[keyBase_ + keyBefore_ + 1u]), keyAt_ - float(keyBefore_)); \
            } \
        } \
    }

uint GlimmerSurfelSunEpoch(GlimmerSurfels surfels)
{
    return uint(surfels.sun.w) & 0xFFFFu;
}

// a sun bounce traced since the sun last jumped is near enough to keep while it follows the sun
bool GlimmerSurfelSunIsCurrent(GlimmerSurfels surfels, uint sunState)
{
    const uint jumpEpoch = uint(surfels.foliage.z) & 0xFFFFu;
    const uint stateEpoch = sunState >> 16;

    return stateEpoch != 0u && ((stateEpoch - jumpEpoch) & 0xFFFFu) <= ((GlimmerSurfelSunEpoch(surfels) - jumpEpoch) & 0xFFFFu);
}

// how much of a surfel's sun bounce is for the sun now: none until it is traced since the sun jumped, all once it has settled
float GlimmerSurfelSunFreshness(GlimmerSurfels surfels, uint sunState)
{
    if (surfels.foliage.w == 0.0 || GlimmerSurfelHasSunKeys(surfels) || !GlimmerSurfelSunIsCurrent(surfels, sunState))
    {
        return 0.0;
    }

    return saturate(float(sunState & 0xFFFFu) / GLIMMER_SURFEL_SUN_SETTLE);
}

// the updates a held sun bounce counts for when a new estimate is blended into it; none when it starts over
uint GlimmerSurfelSunUpdatesHeld(GlimmerSurfels surfels, uint sunState)
{
    if (!GlimmerSurfelSunIsCurrent(surfels, sunState))
    {
        return 0u;
    }

    return sunState & 0xFFFFu;
}

float GlimmerSurfelSunBlendWeight(uint sunUpdates)
{
    return 1.0 / (float(min(sunUpdates, GLIMMER_SURFEL_SUN_HISTORY)) + 1.0);
}

uint GlimmerSurfelNextSunState(GlimmerSurfels surfels, uint sunUpdates)
{
    return (GlimmerSurfelSunEpoch(surfels) << 16) | min(sunUpdates + 1u, 0xFFFFu);
}

uint GlimmerFoliageHashIndex(uint slot)
{
    return slot * 4u;
}

uint GlimmerFoliageIndex(uint voxelIndex, uint field)
{
    return GLIMMER_FOLIAGE_HASH_UINTS + voxelIndex * GLIMMER_FOLIAGE_UINTS + field;
}

// 21 bits an axis
uint2 GlimmerPackFoliageCell(int3 cell)
{
    const uint3 bits = uint3(cell) & 0x1FFFFFu;

    return uint2(bits.x | (bits.y << 21), (bits.y >> 11) | (bits.z << 10));
}

int3 GlimmerUnpackFoliageCell(uint2 packed)
{
    const uint3 bits = uint3(
        packed.x & 0x1FFFFFu,
        (packed.x >> 21) | ((packed.y & 0x3FFu) << 11),
        (packed.y >> 10) & 0x1FFFFFu);

    return int3(bits << 11) >> 11;
}

float3 GlimmerFoliageVoxelCenter(GlimmerSurfels surfels, int3 cell)
{
    return (float3(cell) + 0.5) * surfels.foliage.x;
}

#define GLIMMER_LOAD_FOLIAGE_ENTRY(buffer, voxelIndex, entry) \
    GlimmerUnpackLightEntry(uint2(buffer[GlimmerFoliageIndex(voxelIndex, GLIMMER_FOLIAGE_LIGHT + (entry) * 2u)], buffer[GlimmerFoliageIndex(voxelIndex, GLIMMER_FOLIAGE_LIGHT + (entry) * 2u + 1u)]))

#define GLIMMER_LOAD_FOLIAGE_COMPOSED(buffer, voxelIndex, channel) \
    GlimmerUnpackLightEntry(uint2(buffer[GlimmerFoliageIndex(voxelIndex, GLIMMER_FOLIAGE_COMPOSED + (channel) * 2u)], buffer[GlimmerFoliageIndex(voxelIndex, GLIMMER_FOLIAGE_COMPOSED + (channel) * 2u + 1u)]))

struct GlimmerSurfelLight
{
    float3 parts[GLIMMER_LIGHT_PARTS];
    float direct[GLIMMER_LIGHT_TERMS];
};

uint GlimmerSurfelHash(uint value)
{
    value ^= value >> 16;
    value *= 0x7FEB352Du;
    value ^= value >> 15;
    value *= 0x846CA68Bu;
    value ^= value >> 16;

    return value;
}

uint GlimmerSurfelCellKey(int3 cell)
{
    const uint key = GlimmerSurfelHash(uint(cell.x) * 0x8DA6B343u ^ uint(cell.y) * 0xD8163841u ^ uint(cell.z) * 0xCB1AB31Fu);

    return max(key, 1u);
}

int3 GlimmerSurfelCellOf(GlimmerSurfels surfels, float3 position)
{
    return int3(floor(position * surfels.params.y));
}

uint GlimmerFoliageKey(int3 cell)
{
    const uint key = GlimmerSurfelHash(uint(cell.x) * 0x9E3779B1u ^ uint(cell.y) * 0x85EBCA77u ^ uint(cell.z) * 0xC2B2AE3Du);

    return max(key, 1u);
}

uint GlimmerSurfelCount(uint counter)
{
    return min(counter, GLIMMER_SURFEL_CAPACITY);
}

uint GlimmerFoliageCount(uint counter)
{
    return min(counter, GLIMMER_FOLIAGE_CAPACITY);
}

// what is traced takes turns: the surfels, then the foliage voxels from surfelCount on
uint GlimmerTracedItem(GlimmerSurfels surfels, uint listIndex, uint surfelCount, uint foliageCount)
{
    return (surfels.info.y + listIndex) % max(surfelCount + foliageCount, 1u);
}

// all around, turned each frame
float3 GlimmerFoliageRayDirection(uint voxelIndex, uint frame, uint rayIndex)
{
    const uint seed = GlimmerSurfelHash(voxelIndex * 0x9E3779B9u + frame * 0x85EBCA6Bu + 0x68E31DA4u);

    const float u1 = float(GlimmerSurfelHash(seed) & 0xFFFFFFu) / 16777216.0;
    const float u2 = float(GlimmerSurfelHash(seed + 1u) & 0xFFFFFFu) / 16777216.0;
    const float u3 = float(GlimmerSurfelHash(seed + 2u) & 0xFFFFFFu) / 16777216.0;

    const float a = sqrt(1.0 - u1);
    const float b = sqrt(u1);
    const float twoPi = 6.28318530718;

    const float4 rotation = float4(a * sin(twoPi * u2), a * cos(twoPi * u2), b * sin(twoPi * u3), b * cos(twoPi * u3));

    return normalize(GlimmerRotateByQuaternion(rotation, GlimmerSphericalFibonacci(rayIndex, GLIMMER_SURFEL_RAYS)));
}

uint GlimmerPackSurfelNormal(float3 N)
{
    const uint2 quantized = uint2(round(saturate(GlimmerOctahedralEncode(N)) * 65535.0));

    return quantized.x | (quantized.y << 16);
}

float3 GlimmerUnpackSurfelNormal(uint packed)
{
    return GlimmerOctahedralDecode(float2(packed & 0xFFFFu, packed >> 16) * (1.0 / 65535.0));
}

void GlimmerSurfelBasis(float3 N, out float3 outT, out float3 outB)
{
    const float s = N.z >= 0.0 ? 1.0 : -1.0;
    const float a = -1.0 / (s + N.z);
    const float b = N.x * N.y * a;

    outT = float3(1.0 + s * N.x * N.x * a, s * b, -s * N.x);
    outB = float3(b, s + N.y * N.y * a, -N.y);
}

// cosine weighted around N, stratified, turned and jittered each frame
float3 GlimmerSurfelRayDirection(float3 N, uint surfelIndex, uint frame, uint rayIndex)
{
    const uint seed = GlimmerSurfelHash(surfelIndex * 0x9E3779B9u + frame * 0x85EBCA6Bu);

    const float turn = float(seed & 0xFFFFu) * (1.0 / 65536.0);
    const float jitter = float(seed >> 16) * (1.0 / 65536.0);

    const float u = (float(rayIndex) + jitter) / float(GLIMMER_SURFEL_RAYS);
    const float phi = 6.28318530718 * frac(float(rayIndex) * 0.61803398875 + turn);
    const float r = sqrt(u);

    float3 T;
    float3 B;
    GlimmerSurfelBasis(N, T, B);

    return normalize(T * (r * cos(phi)) + B * (r * sin(phi)) + N * sqrt(saturate(1.0 - u)));
}

float3 GlimmerSunIrradiance(GlimmerLightParams params)
{
    return params.sun.rgb;
}

// the sun bounce, as fresh as it is, stands in for the SH sun's share of the bounce
float3 GlimmerComposeSurfelLight(GlimmerLightParams params, GlimmerSurfelLight light, float3 sun, float sunFreshness)
{
    float3 irradiance = light.parts[GLIMMER_LIGHT_REST] + GlimmerSunIrradiance(params) * sun * sunFreshness;

    [unroll]
    for (uint term = 0; term < GLIMMER_LIGHT_TERMS; term++)
    {
        const float3 bounceWeight = lerp(params.bounceWeights[term].rgb, params.directWeights[term].rgb, sunFreshness);

        irradiance += light.parts[term] * bounceWeight + params.directWeights[term].rgb * light.direct[term];
    }

    return irradiance;
}

// GlimmerComposeStoredLight, with the sun bounce (L1 per colour channel), as fresh as it is, standing in for the SH sun's share of the bounce
void GlimmerComposeStoredLightWithSun(GlimmerLightParams params, float4 entries[GLIMMER_LIGHT_ENTRIES], float4 sun[3], float sunFreshness, out float4 outComposed[3])
{
    const float3 sunIrradiance = GlimmerSunIrradiance(params) * sunFreshness;

    [unroll]
    for (uint channel = 0; channel < 3; channel++)
    {
        outComposed[channel] = entries[GLIMMER_LIGHT_REST_ENTRY + channel] + sun[channel] * sunIrradiance[channel];
    }

    [unroll]
    for (uint term = 0; term < GLIMMER_LIGHT_TERMS; term++)
    {
        const float3 bounceWeight = lerp(params.bounceWeights[term].rgb, params.directWeights[term].rgb, sunFreshness);
        const float3 directWeight = params.directWeights[term].rgb;

        const float4 direct = entries[GLIMMER_LIGHT_DIRECT_ENTRY + term];

        outComposed[0] += GlimmerGetBounceEntry(entries, term, 0u) * bounceWeight.r + direct * directWeight.r;
        outComposed[1] += GlimmerGetBounceEntry(entries, term, 1u) * bounceWeight.g + direct * directWeight.g;
        outComposed[2] += GlimmerGetBounceEntry(entries, term, 2u) * bounceWeight.b + direct * directWeight.b;
    }
}

// the sun a light's terms carry, per unit of its irradiance, as the bounce weights spread it over the terms
float3 GlimmerSmoothSun(GlimmerLight light, float3 toSun)
{
    float sunTerms[GLIMMER_LIGHT_TERMS];
    GlimmerSunTerms(toSun, sunTerms);

    float3 total = (float3)0.0;

    [unroll]
    for (uint term = 0; term < GLIMMER_LIGHT_TERMS; term++)
    {
        total += light.parts[term] * sunTerms[term];
    }

    return total;
}

// a held sun bounce: L1 per colour channel, then the sun state
#define GLIMMER_LOAD_SUN_BOUNCE(buffer, base, outSun, outState)                                                \
    {                                                                                                          \
        [unroll]                                                                                               \
        for (uint sunChannel = 0; sunChannel < 3; sunChannel++)                                                \
        {                                                                                                      \
            outSun[sunChannel] = GlimmerUnpackLightEntry(uint2(buffer[(base) + sunChannel * 2u], buffer[(base) + sunChannel * 2u + 1u])); \
        }                                                                                                      \
        outState = buffer[(base) + GLIMMER_SUN_BOUNCE_STATE];                                                  \
    }

#define GLIMMER_STORE_SUN_BOUNCE(buffer, base, sun, state)                                                     \
    {                                                                                                          \
        [unroll]                                                                                               \
        for (uint sunChannel = 0; sunChannel < 3; sunChannel++)                                                \
        {                                                                                                      \
            const uint2 packedSun = GlimmerPackLightEntry(sun[sunChannel]);                                    \
            buffer[(base) + sunChannel * 2u] = packedSun.x;                                                    \
            buffer[(base) + sunChannel * 2u + 1u] = packedSun.y;                                               \
        }                                                                                                      \
        buffer[(base) + GLIMMER_SUN_BOUNCE_STATE] = state;                                                     \
    }

// a held sun bounce's irradiance facing N, as fresh as it is
float3 GlimmerEvaluateSunBounce(float4 sun[3], float freshness, float3 N)
{
    return float3(GlimmerEvaluateLinearL1(sun[0], N), GlimmerEvaluateLinearL1(sun[1], N), GlimmerEvaluateLinearL1(sun[2], N)) * freshness;
}

uint2 GlimmerPackSurfelComposed(float3 irradiance, float weight)
{
    return GlimmerPackLightEntry(float4(max(irradiance, (float3)0.0), weight));
}

float4 GlimmerUnpackSurfelComposed(uint2 packed)
{
    return GlimmerUnpackLightEntry(packed);
}

#endif
