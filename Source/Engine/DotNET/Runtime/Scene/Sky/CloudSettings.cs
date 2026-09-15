using System;
using System.Runtime.InteropServices;

namespace Hyperion
{
    // field order and types must match CloudSettings in Scene/Sky/CloudSettings.hpp
    [ClassBinding(Name = "CloudSettings")]
    [StructLayout(LayoutKind.Sequential)]
    public struct CloudSettings
    {
        [MarshalAs(UnmanagedType.I1)]
        public bool enabled;

        public float coverage;
        public float cloudTypeBias;
        public float densityMultiplier;
        public float baseAltitude;
        public float layerThickness;
        public float weatherScale;
        public float cloudSize;
        public float shapeNoiseScale;
        public float detailNoiseScale;
        public float detailErosion;
        public float windDirectionDegrees;
        public float windSpeed;
        public float evolutionSpeed;
        public uint seed;
        public float shadowStrength;
        public float shadowSoftness;
        public float hazeDistance;

        public CloudSettings()
        {
            enabled = true;
            coverage = 0.5f;
            cloudTypeBias = 0.6f;
            densityMultiplier = 1.0f;
            baseAltitude = 1500.0f;
            layerThickness = 2500.0f;
            weatherScale = 12000.0f;
            cloudSize = 1500.0f;
            shapeNoiseScale = 3000.0f;
            detailNoiseScale = 400.0f;
            detailErosion = 0.35f;
            windDirectionDegrees = 45.0f;
            windSpeed = 10.0f;
            evolutionSpeed = 1.0f;
            seed = 0;
            shadowStrength = 0.85f;
            shadowSoftness = 0.5f;
            hazeDistance = 40000.0f;
        }
    }
}
