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
    float diffuseIntensity = 1.0f;

    HYP_FIELD(Property = "SpecularIntensity", Serialize, Label = "Specular Intensity")
    float specularIntensity = 1.0f;
};

HYP_STRUCT()
struct ExposureSettings
{
    HYP_STRUCT_BODY(ExposureSettings);

    HYP_FIELD(Property = "ExposureCompensation", Serialize, Label = "Exposure Compensation")
    float exposureCompensation = 1.0f;

    HYP_FIELD(Property = "TonemapOperator", Serialize, Label = "Tonemapper")
    TonemapOperator tonemapOperator = TonemapOperator::ACES;

    HYP_FIELD(Property = "WhiteBalanceTemperature", Serialize, Label = "Temperature")
    float whiteBalanceTemperature = 0.0f;

    HYP_FIELD(Property = "WhiteBalanceTint", Serialize, Label = "Tint")
    float whiteBalanceTint = 0.0f;

    HYP_FIELD(Property = "Saturation", Serialize, Label = "Saturation")
    float saturation = 1.15f;

    HYP_FIELD(Property = "Contrast", Serialize, Label = "Contrast")
    float contrast = 1.0f;
};

HYP_STRUCT()
struct HeightFogSettings
{
    HYP_STRUCT_BODY(HeightFogSettings);

    HYP_FIELD(Property = "Enabled", Serialize, Label = "Enabled")
    bool enabled = true;

    HYP_FIELD(Property = "Density", Serialize, Label = "Density")
    float density = 0.0012f;

    HYP_FIELD(Property = "HeightFalloff", Serialize, Label = "Height Falloff")
    float heightFalloff = 0.05f;

    HYP_FIELD(Property = "BaseHeight", Serialize, Label = "Base Height")
    float baseHeight = 0.0f;

    HYP_FIELD(Property = "StartDistance", Serialize, Label = "Start Distance")
    float startDistance = 50.0f;

    HYP_FIELD(Property = "AerialPerspectiveDistance", Serialize, Label = "Aerial Perspective Distance")
    float aerialPerspectiveDistance = 6000.0f;

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
};

void WriteEnvironmentShaderData(
    const EnvironmentSettings& settings,
    WorldShaderData& outShaderData);

} // namespace Hyperion
