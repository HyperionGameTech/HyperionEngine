using System;
using System.Runtime.InteropServices;

namespace Hyperion
{
    [ClassBinding(Name = "LightType")]
    public enum LightType : uint
    {
        Directional = 0,
        Point,
        Spot,
        AreaRect,
        Count
    }

    [ClassBinding(Name = "LightFlags")]
    [Flags]
    public enum LightFlags : uint
    {
        None = 0x0,                                     //!< @editor=false

        ShadowCaster = 0x1,                             //!< @title="Render shadows"

        CacheStaticShadowMaps = 0x10,                   //!< @title="Cache shadow maps for static objects"
        BakeStaticShadows = 0x20,                       //!< @editor=false

        OnlyDrawStaticShadowMaps = 0x40,                //!< @title="Only render shadows for static objects"

        Default = ShadowCaster | CacheStaticShadowMaps  //!< @editor=false
    }

    [ClassBinding(Name = "Light")]
    public class Light : Entity
    {
        public Light()
        {
        }

        public LightType Type
        {
            get
            {
                return this.GetLightType();
            }
        }

        public Color Color
        {
            get
            {
                return this.GetColor();
            }
            set
            {
                this.SetColor(value);
            }
        }

        public float Intensity
        {
            get
            {
                return this.GetIntensity();
            }
            set
            {
                this.SetIntensity(value);
            }
        }

        public Vec2u ShadowMapDimensions
        {
            get
            {
                return this.GetShadowMapDimensions();
            }
            set
            {
                this.SetShadowMapDimensions(value);
            }
        }
    }

    [ClassBinding(Name = "DirectionalLight")]
    public class DirectionalLight : Light
    {
        public DirectionalLight()
        {
        }

        public Vec3f Direction
        {
            get
            {
                return this.GetDirection();
            }
            set
            {
                this.SetDirection(value);
            }
        }
    }

    [ClassBinding(Name = "PointLight")]
    public class PointLight : Light
    {
        public PointLight()
        {
        }

        public float Radius
        {
            get
            {
                return this.GetRadius();
            }
            set
            {
                this.SetRadius(value);
            }
        }

        public float Falloff
        {
            get
            {
                return this.GetFalloff();
            }
            set
            {
                this.SetFalloff(value);
            }
        }
    }

    [ClassBinding(Name = "SpotLight")]
    public class SpotLight : Light
    {
        public SpotLight()
        {
        }

        public Vec2f SpotAngles
        {
            get
            {
                return this.GetSpotAngles();
            }
            set
            {
                this.SetSpotAngles(value);
            }
        }

        public float Radius
        {
            get
            {
                return this.GetRadius();
            }
            set
            {
                this.SetRadius(value);
            }
        }

        public float Falloff
        {
            get
            {
                return this.GetFalloff();
            }
            set
            {
                this.SetFalloff(value);
            }
        }

        public Vec2f AreaSize
        {
            get
            {
                return this.GetAreaSize();
            }
            set
            {
                this.SetAreaSize(value);
            }
        }
    }

    [ClassBinding(Name = "AreaRectLight")]
    public class AreaRectLight : Light
    {
        public AreaRectLight()
        {
        }

        public Vec2f AreaSize
        {
            get
            {
                return this.GetAreaSize();
            }
            set
            {
                this.SetAreaSize(value);
            }
        }

        public Material Material
        {
            get
            {
                return this.GetMaterial();
            }
            set
            {
                this.SetMaterial(value);
            }
        }
    }
}
