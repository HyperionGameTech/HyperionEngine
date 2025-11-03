using System;
using System.Runtime.InteropServices;

namespace Hyperion
{
    [ClassBinding(Name = "LightType")]
    public enum LightType : uint
    {
        Directional = 0,
        Point = 1,
        Spot = 2,
        AreaRect = 3
    }

    [ClassBinding(Name = "LightFlags")]
    [Flags]
    public enum LightFlags : uint
    {
        None = 0,
        Shadow = 0x1,
        ShadowFilterPcf = 0x2,
        ShadowFilterContactHardening = 0x4,
        ShadowFilterVariance = 0x8,
        ShadowFilterMask = (ShadowFilterPcf | ShadowFilterContactHardening | ShadowFilterVariance),

        Default = Shadow | ShadowFilterPcf
    }

    [ClassBinding(Name = "Light")]
    public class Light : Entity
    {
        public Light()
        {
        }

        ~Light()
        {
            Logger.Log(LogType.Warn, "Destroying Light with Id: {0}", this.Id);
        }
    }

    [ClassBinding(Name = "DirectionalLight")]
    public class DirectionalLight : Light
    {
        public DirectionalLight()
        {
        }
    }

    [ClassBinding(Name = "PointLight")]
    public class PointLight : Light
    {
        public PointLight()
        {
        }
    }

    [ClassBinding(Name = "SpotLight")]
    public class SpotLight : Light
    {
        public SpotLight()
        {
        }
    }

    [ClassBinding(Name = "AreaRectLight")]
    public class AreaRectLight : Light
    {
        public AreaRectLight()
        {
        }
    }
}
