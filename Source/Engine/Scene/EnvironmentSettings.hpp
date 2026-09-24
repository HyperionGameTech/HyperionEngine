/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#pragma once

#include <Core/Reflection/ObjectMacros.hpp>

#include <Core/Math/Vector3.hpp>

#include <Core/Types.hpp>

namespace Hyperion {

struct WorldShaderData;

HYP_ENUM()
enum class TonemapOperator : uint32
{
    AgX = 0,
    AgXPunchy,
    ACES,
    PBRNeutral,
    Reinhard
};

HYP_STRUCT()
struct CloudLayerSettings
{
    HYP_STRUCT_BODY(CloudLayerSettings);

    HYP_FIELD(Property = "BaseAltitude", Serialize, Label = "Base Altitude")
    float baseAltitude = 1500.0f;

    HYP_FIELD(Property = "Thickness", Serialize, Label = "Thickness")
    float thickness = 1000.0f;
};

HYP_STRUCT()
struct CloudShapeSettings
{
    HYP_STRUCT_BODY(CloudShapeSettings);

    HYP_FIELD(Property = "Coverage", Serialize, Label = "Cloud Coverage")
    float coverage = 0.85f;

    HYP_FIELD(Property = "CloudTypeBias", Serialize, Label = "Cloud Type Bias")
    float cloudTypeBias = 0.9f;

    HYP_FIELD(Property = "DensityMultiplier", Serialize, Label = "Density Multiplier")
    float densityMultiplier = 1.0f;

    HYP_FIELD(Property = "DetailErosion", Serialize, Label = "Detail Erosion")
    float detailErosion = 0.3f;
};

HYP_STRUCT()
struct CloudNoiseSettings
{
    HYP_STRUCT_BODY(CloudNoiseSettings);

    HYP_FIELD(Property = "WeatherScale", Serialize, Label = "Weather Scale")
    float weatherScale = 12000.0f;

    HYP_FIELD(Property = "CloudSize", Serialize, Label = "Cloud Size")
    float cloudSize = 1500.0f;

    HYP_FIELD(Property = "ShapeScale", Serialize, Label = "Shape Scale")
    float shapeScale = 3000.0f;

    HYP_FIELD(Property = "DetailScale", Serialize, Label = "Detail Scale")
    float detailScale = 250.0f;

    HYP_FIELD(Property = "Seed", Serialize, Label = "Seed")
    uint32 seed = 0;
};

HYP_STRUCT()
struct CloudWindSettings
{
    HYP_STRUCT_BODY(CloudWindSettings);

    HYP_FIELD(Property = "Direction", Serialize, Label = "Direction")
    float directionDegrees = 45.0f;

    HYP_FIELD(Property = "Speed", Serialize, Label = "Speed")
    float speed = 10.0f;

    HYP_FIELD(Property = "EvolutionSpeed", Serialize, Label = "Evolution Speed")
    float evolutionSpeed = 1.0f;
};

HYP_STRUCT()
struct CloudLightingSettings
{
    HYP_STRUCT_BODY(CloudLightingSettings);

    HYP_FIELD(Property = "ShadowStrength", Serialize, Label = "Shadow Strength")
    float shadowStrength = 0.85f;

    HYP_FIELD(Property = "ShadowSoftness", Serialize, Label = "Shadow Softness")
    float shadowSoftness = 0.2f;

    HYP_FIELD(Property = "HazeDistance", Serialize, Label = "Haze Distance")
    float hazeDistance = 40000.0f;
};

HYP_STRUCT()
struct CloudSettings
{
    HYP_STRUCT_BODY(CloudSettings);

    HYP_FIELD(Property = "Enabled", Serialize, Label = "Enabled")
    bool enabled = true;

    HYP_FIELD(Property = "Layer", Serialize, Label = "Layer")
    CloudLayerSettings layer;

    HYP_FIELD(Property = "Shape", Serialize, Label = "Shape")
    CloudShapeSettings shape;

    HYP_FIELD(Property = "Noise", Serialize, Label = "Noise")
    CloudNoiseSettings noise;

    HYP_FIELD(Property = "Wind", Serialize, Label = "Wind")
    CloudWindSettings wind;

    HYP_FIELD(Property = "Lighting", Serialize, Label = "Lighting")
    CloudLightingSettings lighting;
};

HYP_STRUCT()
struct SkyLookSettings
{
    HYP_STRUCT_BODY(SkyLookSettings);

    HYP_FIELD(Property = "Tint", Serialize, Label = "Tint")
    Vec3f tint = Vec3f(1.0f);

    HYP_FIELD(Property = "Intensity", Serialize, Label = "Intensity")
    float intensity = 1.0f;

    HYP_FIELD(Property = "OvercastBrightness", Serialize, Label = "Overcast Brightness")
    float overcastBrightness = 1.0f;

    HYP_FIELD(Property = "ShowSunDisk", Serialize, Label = "Show Sun Disk")
    bool showSunDisk = true;
};

HYP_STRUCT()
struct SkyLightSettings
{
    HYP_STRUCT_BODY(SkyLightSettings);

    HYP_FIELD(Property = "DiffuseIntensity", Serialize, Label = "Diffuse Intensity")
    float diffuseIntensity = 1.3f;

    HYP_FIELD(Property = "SpecularIntensity", Serialize, Label = "Specular Intensity")
    float specularIntensity = 1.0f;

    HYP_FIELD(Property = "OcclusionStrength", Serialize, Label = "Occlusion Strength")
    float occlusionStrength = 1.0f;

    HYP_FIELD(Property = "OcclusionRadius", Serialize, Label = "Occlusion Radius")
    float occlusionRadius = 3.0f;

    HYP_FIELD(Property = "OcclusionBias", Serialize, Label = "Occlusion Bias")
    float occlusionBias = 0.5f;
};

HYP_STRUCT()
struct ExposureSettings
{
    HYP_STRUCT_BODY(ExposureSettings);

    HYP_FIELD(Property = "ExposureCompensation", Serialize, Label = "Exposure Compensation")
    float exposureCompensation = 1.6f;

    HYP_FIELD(Property = "TonemapOperator", Serialize, Label = "Tonemapper")
    TonemapOperator tonemapOperator = TonemapOperator::AgX;

    HYP_FIELD(Property = "WhiteBalanceTemperature", Serialize, Label = "Temperature")
    float whiteBalanceTemperature = 0.0f;

    HYP_FIELD(Property = "WhiteBalanceTint", Serialize, Label = "Tint")
    float whiteBalanceTint = 0.0f;

    HYP_FIELD(Property = "Saturation", Serialize, Label = "Saturation")
    float saturation = 1.05f;

    HYP_FIELD(Property = "Contrast", Serialize, Label = "Contrast")
    float contrast = 1.05f;
};

HYP_STRUCT()
struct HeightFogSettings
{
    HYP_STRUCT_BODY(HeightFogSettings);

    HYP_FIELD(Property = "Enabled", Serialize, Label = "Enabled")
    bool enabled = true;

    HYP_FIELD(Property = "Density", Serialize, Label = "Density")
    float density = 0.003f;

    HYP_FIELD(Property = "HeightFalloff", Serialize, Label = "Height Falloff")
    float heightFalloff = 0.05f;

    HYP_FIELD(Property = "BaseHeight", Serialize, Label = "Base Height")
    float baseHeight = 0.0f;

    HYP_FIELD(Property = "StartDistance", Serialize, Label = "Start Distance")
    float startDistance = 50.0f;

    HYP_FIELD(Property = "AerialPerspectiveDistance", Serialize, Label = "Aerial Perspective Distance")
    float aerialPerspectiveDistance = 4500.0f;

    HYP_FIELD(Property = "SkyInscatter", Serialize, Label = "Sky Inscatter")
    float skyInscatterStrength = 1.0f;

    HYP_FIELD(Property = "SunInscatter", Serialize, Label = "Sun Inscatter")
    float sunInscatterStrength = 0.25f;

    HYP_FIELD(Property = "SunAnisotropy", Serialize, Label = "Sun Anisotropy")
    float sunAnisotropy = 0.6f;

    HYP_FIELD(Property = "MaxOpacity", Serialize, Label = "Max Opacity")
    float maxOpacity = 1.0f;
};

HYP_STRUCT()
struct WindSettings
{
    HYP_STRUCT_BODY(WindSettings);

    // bearing the wind blows toward, in degrees, the same way round as the clouds' wind
    HYP_FIELD(Property = "Direction", Serialize, Label = "Direction")
    float directionDegrees = 45.0f;

    // 1 is a full gale
    HYP_FIELD(Property = "Strength", Serialize, Label = "Strength")
    float strength = 0.3f;

    HYP_FIELD(Property = "Gustiness", Serialize, Label = "Gustiness")
    float gustiness = 0.5f;
};

HYP_STRUCT()
struct GlobalIlluminationSettings
{
    HYP_STRUCT_BODY(GlobalIlluminationSettings);

    HYP_FIELD(Property = "DDGIEnabled", Serialize, Label = "Dynamic Diffuse GI (DDGI)")
    bool ddgiEnabled = true;

    HYP_FIELD(Property = "RayTracedReflectionsEnabled", Serialize, Label = "Ray Traced Reflections")
    bool rayTracedReflectionsEnabled = true;
};

HYP_STRUCT()
struct EnvironmentSettings
{
    HYP_STRUCT_BODY(EnvironmentSettings);

    HYP_FIELD(Property = "Sky", Serialize, Label = "Sky")
    SkyLookSettings sky;

    HYP_FIELD(Property = "SkyLight", Serialize, Label = "Sky Light")
    SkyLightSettings skyLight;

    HYP_FIELD(Property = "Exposure", Serialize, Label = "Exposure & Color")
    ExposureSettings exposure;

    HYP_FIELD(Property = "HeightFog", Serialize, Label = "Height Fog")
    HeightFogSettings heightFog;

    HYP_FIELD(Property = "Clouds", Serialize, Label  = "Clouds")
    CloudSettings clouds;

    HYP_FIELD(Property = "Wind", Serialize, Label = "Wind")
    WindSettings wind;

    HYP_FIELD(Property = "GlobalIllumination", Serialize, Label = "Global Illumination & Reflections")
    GlobalIlluminationSettings globalIllumination;
};

void WriteEnvironmentShaderData(
    const EnvironmentSettings& settings,
    WorldShaderData& outShaderData);

} // namespace Hyperion
