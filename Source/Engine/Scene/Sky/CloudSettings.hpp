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

} // namespace Hyperion
