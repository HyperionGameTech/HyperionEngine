/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#pragma once

#include <Core/Reflection/ObjectMacros.hpp>

#include <Core/Types.hpp>

namespace Hyperion {

HYP_STRUCT()
struct CloudSettings
{
    HYP_STRUCT_BODY(CloudSettings);

    HYP_FIELD(Property = "Enabled", Serialize, Label = "Enabled")
    bool enabled = true;

    HYP_FIELD(Property = "Coverage", Serialize, Label = "Cloud Coverage")
    float coverage = 0.6f;

    HYP_FIELD(Property = "CloudTypeBias", Serialize, Label = "Cloud Type Bias")
    float cloudTypeBias = 0.9f;

    HYP_FIELD(Property = "DensityMultiplier", Serialize, Label = "Density Multiplier")
    float densityMultiplier = 1.0f;

    HYP_FIELD(Property = "BaseAltitude", Serialize, Label = "Base Altitude")
    float baseAltitude = 1500.0f;

    HYP_FIELD(Property = "LayerThickness", Serialize, Label = "Layer Thickness")
    float layerThickness = 1000.0f;

    HYP_FIELD(Property = "WeatherScale", Serialize, Label = "Weather Scale")
    float weatherScale = 12000.0f;

    HYP_FIELD(Property = "CloudSize", Serialize, Label = "Cloud Size")
    float cloudSize = 1500.0f;

    HYP_FIELD(Property = "ShapeNoiseScale", Serialize, Label = "Shape Noise Scale")
    float shapeNoiseScale = 3000.0f;

    HYP_FIELD(Property = "DetailNoiseScale", Serialize, Label = "Detail Noise Scale")
    float detailNoiseScale = 250.0f;

    HYP_FIELD(Property = "DetailErosion", Serialize, Label = "Detail Erosion")
    float detailErosion = 0.8f;

    HYP_FIELD(Property = "WindDirection", Serialize, Label = "Wind Direction")
    float windDirectionDegrees = 45.0f;

    HYP_FIELD(Property = "WindSpeed", Serialize, Label = "Wind Speed")
    float windSpeed = 10.0f;

    HYP_FIELD(Property = "EvolutionSpeed", Serialize, Label = "Evolution Speed")
    float evolutionSpeed = 1.0f;

    HYP_FIELD(Property = "Seed", Serialize, Label = "Seed")
    uint32 seed = 0;

    HYP_FIELD(Property = "ShadowStrength", Serialize, Label = "Shadow Strength")
    float shadowStrength = 0.85f;

    HYP_FIELD(Property = "ShadowSoftness", Serialize, Label = "Shadow Softness")
    float shadowSoftness = 0.2f;

    HYP_FIELD(Property = "HazeDistance", Serialize, Label = "Haze Distance")
    float hazeDistance = 40000.0f;
};

} // namespace Hyperion
