using System;
using System.Runtime.InteropServices;

namespace Hyperion
{
    // field order and types must match the structs in Scene/Sky/CloudSettings.hpp
    [ClassBinding(Name = "CloudLayerSettings")]
    [StructLayout(LayoutKind.Sequential)]
    public struct CloudLayerSettings
    {
        public float baseAltitude;
        public float thickness;

        public CloudLayerSettings()
        {
            baseAltitude = 1500.0f;
            thickness = 1000.0f;
        }
    }

    [ClassBinding(Name = "CloudShapeSettings")]
    [StructLayout(LayoutKind.Sequential)]
    public struct CloudShapeSettings
    {
        public float coverage;
        public float cloudTypeBias;
        public float densityMultiplier;
        public float detailErosion;

        public CloudShapeSettings()
        {
            coverage = 0.6f;
            cloudTypeBias = 0.9f;
            densityMultiplier = 1.0f;
            detailErosion = 0.8f;
        }
    }

    [ClassBinding(Name = "CloudNoiseSettings")]
    [StructLayout(LayoutKind.Sequential)]
    public struct CloudNoiseSettings
    {
        public float weatherScale;
        public float cloudSize;
        public float shapeScale;
        public float detailScale;
        public uint seed;

        public CloudNoiseSettings()
        {
            weatherScale = 12000.0f;
            cloudSize = 1500.0f;
            shapeScale = 3000.0f;
            detailScale = 250.0f;
            seed = 0;
        }
    }

    [ClassBinding(Name = "CloudWindSettings")]
    [StructLayout(LayoutKind.Sequential)]
    public struct CloudWindSettings
    {
        public float directionDegrees;
        public float speed;
        public float evolutionSpeed;

        public CloudWindSettings()
        {
            directionDegrees = 45.0f;
            speed = 10.0f;
            evolutionSpeed = 1.0f;
        }
    }

    [ClassBinding(Name = "CloudLightingSettings")]
    [StructLayout(LayoutKind.Sequential)]
    public struct CloudLightingSettings
    {
        public float shadowStrength;
        public float shadowSoftness;
        public float hazeDistance;

        public CloudLightingSettings()
        {
            shadowStrength = 0.85f;
            shadowSoftness = 0.2f;
            hazeDistance = 40000.0f;
        }
    }

    [ClassBinding(Name = "CloudSettings")]
    [StructLayout(LayoutKind.Sequential)]
    public struct CloudSettings
    {
        [MarshalAs(UnmanagedType.I1)]
        public bool enabled;

        public CloudLayerSettings layer;
        public CloudShapeSettings shape;
        public CloudNoiseSettings noise;
        public CloudWindSettings wind;
        public CloudLightingSettings lighting;

        public CloudSettings()
        {
            enabled = true;
            layer = new CloudLayerSettings();
            shape = new CloudShapeSettings();
            noise = new CloudNoiseSettings();
            wind = new CloudWindSettings();
            lighting = new CloudLightingSettings();
        }
    }
}
