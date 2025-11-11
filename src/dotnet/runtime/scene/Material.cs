using System;
using System.Runtime.InteropServices;

namespace Hyperion
{
    [ClassBinding(Name = "MaterialParameterKey")]
    public enum MaterialParameterKey : ulong
    {
        None = 0x0,
        Albedo = 0x1,
        Metalness = 0x2,
        Roughness = 0x4,
        Transmission = 0x8,
        Emissive = 0x10,
        Specular = 0x20,
        SpecularTint = 0x40,
        Anisotropic = 0x80,
        Sheen = 0x100,
        SheenTint = 0x200,
        Clearcoat = 0x400,
        ClearcoatGloss = 0x800,
        Subsurface = 0x1000,
        NormalMapIntensity = 0x2000,
        UVScale = 0x4000,
        ParallaxHeight = 0x8000,
        AlphaThreshold = 0x10000
    }

    public enum MaterialParameterType : uint
    {
        None = 0,
        Float = 1,
        Float2 = 2,
        Float3 = 3,
        Float4 = 4,
        Int = 5,
        Int2 = 6,
        Int3 = 7,
        Int4 = 8
    }

    [StructLayout(LayoutKind.Explicit, Size = 16)]
    public struct MaterialParameter
    {
        // natively represented as a union of float and int

        [FieldOffset(0)]
        private float f0;
        [FieldOffset(4)]
        private float f1;
        [FieldOffset(8)]
        private float f2;
        [FieldOffset(12)]
        private float f3;

        [FieldOffset(0)]
        private int i0;
        [FieldOffset(4)]
        private int i1;
        [FieldOffset(8)]
        private int i2;
        [FieldOffset(12)]
        private int i3;

        public MaterialParameter()
        {
            this.f0 = 0;
            this.f1 = 0;
            this.f2 = 0;
            this.f3 = 0;
        }

        public MaterialParameter(float f0, float f1, float f2, float f3)
        {
            this.f0 = f0;
            this.f1 = f1;
            this.f2 = f2;
            this.f3 = f3;
        }

        public MaterialParameter(float f0, float f1, float f2)
        {
            this.f0 = f0;
            this.f1 = f1;
            this.f2 = f2;
            this.f3 = 0;
        }

        public MaterialParameter(float f0, float f1)
        {
            this.f0 = f0;
            this.f1 = f1;
            this.f2 = 0;
            this.f3 = 0;
        }

        public MaterialParameter(float f0)
        {
            this.f0 = f0;
            this.f1 = 0;
            this.f2 = 0;
            this.f3 = 0;
        }

        public MaterialParameter(int i0, int i1, int i2, int i3)
        {
            this.i0 = i0;
            this.i1 = i1;
            this.i2 = i2;
            this.i3 = i3;
        }

        public MaterialParameter(int i0, int i1, int i2)
        {
            this.i0 = i0;
            this.i1 = i1;
            this.i2 = i2;
            this.i3 = 0;
        }

        public MaterialParameter(int i0, int i1)
        {
            this.i0 = i0;
            this.i1 = i1;
            this.i2 = 0;
            this.i3 = 0;
        }

        public MaterialParameter(int i0)
        {
            this.i0 = i0;
            this.i1 = 0;
            this.i2 = 0;
            this.i3 = 0;
        }
    }

    public enum TextureKey : ulong
    {
        None = 0x0,
        AlbedoMap = 0x1,
        NormalMap = 0x2,
        ParallaxMap = 0x4,
        MetalnessMap = 0x8,
        RoughnessMap = 0x10,
        AOMap = 0x20,
        SkyboxMap = 0x40,
        ColorMap = 0x80,
        PositionMap = 0x100,
        DataMap = 0x200,
        SSAOMap = 0x400,
        TangentMap = 0x800,
        BitangentMap = 0x1000,
        DepthMap = 0x2000
    }

    [ClassBinding(Name = "Material")]
    public class Material : ObjectBase
    {
        public Material()
        {
        }
    }
}