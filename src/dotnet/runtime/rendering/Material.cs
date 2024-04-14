using System;
using System.Runtime.InteropServices;

namespace Hyperion
{
    public enum MaterialKey : ulong
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

    [StructLayout(LayoutKind.Explicit, Size = 20)]
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

        [FieldOffset(16)]
        private MaterialParameterType type;

        public MaterialParameter()
        {
            this.f0 = 0;
            this.f1 = 0;
            this.f2 = 0;
            this.f3 = 0;

            this.type = MaterialParameterType.None;
        }

        public MaterialParameter(float f0, float f1, float f2, float f3)
        {
            this.f0 = f0;
            this.f1 = f1;
            this.f2 = f2;
            this.f3 = f3;

            this.type = MaterialParameterType.Float4;
        }

        public MaterialParameter(float f0, float f1, float f2)
        {
            this.f0 = f0;
            this.f1 = f1;
            this.f2 = f2;
            this.f3 = 0;

            this.type = MaterialParameterType.Float3;
        }

        public MaterialParameter(float f0, float f1)
        {
            this.f0 = f0;
            this.f1 = f1;
            this.f2 = 0;
            this.f3 = 0;

            this.type = MaterialParameterType.Float2;
        }

        public MaterialParameter(float f0)
        {
            this.f0 = f0;
            this.f1 = 0;
            this.f2 = 0;
            this.f3 = 0;

            this.type = MaterialParameterType.Float;
        }

        public MaterialParameter(int i0, int i1, int i2, int i3)
        {
            this.i0 = i0;
            this.i1 = i1;
            this.i2 = i2;
            this.i3 = i3;

            this.type = MaterialParameterType.Int4;
        }

        public MaterialParameter(int i0, int i1, int i2)
        {
            this.i0 = i0;
            this.i1 = i1;
            this.i2 = i2;
            this.i3 = 0;

            this.type = MaterialParameterType.Int3;
        }

        public MaterialParameter(int i0, int i1)
        {
            this.i0 = i0;
            this.i1 = i1;
            this.i2 = 0;
            this.i3 = 0;

            this.type = MaterialParameterType.Int2;
        }

        public MaterialParameter(int i0)
        {
            this.i0 = i0;
            this.i1 = 0;
            this.i2 = 0;
            this.i3 = 0;

            this.type = MaterialParameterType.Int;
        }

        public MaterialParameterType Type
        {
            get
            {
                return type;
            }
        }

        public override string ToString()
        {
            if (type == MaterialParameterType.Float)
            {
                return f0.ToString();
            }
            else if (type == MaterialParameterType.Float2)
            {
                return $"({f0}, {f1})";
            }
            else if (type == MaterialParameterType.Float3)
            {
                return $"({f0}, {f1}, {f2})";
            }
            else if (type == MaterialParameterType.Float4)
            {
                return $"({f0}, {f1}, {f2}, {f3})";
            }
            else if (type == MaterialParameterType.Int)
            {
                return i0.ToString();
            }
            else if (type == MaterialParameterType.Int2)
            {
                return $"({i0}, {i1})";
            }
            else if (type == MaterialParameterType.Int3)
            {
                return $"({i0}, {i1}, {i2})";
            }
            else if (type == MaterialParameterType.Int4)
            {
                return $"({i0}, {i1}, {i2}, {i3})";
            }
            else
            {
                return "";
            }
        }
    }

    public enum TextureKey : ulong
    {
        None = 0x0,
        AlbedoMap = 0x1,
        NormalMap = 0x2,
        AOMap = 0x4,
        ParallaxMap = 0x8,
        MetalnessMap = 0x10,
        RoughnessMap = 0x20,
        SkyboxMap = 0x40,
        ColorMap = 0x80,
        PositionMap = 0x100,
        DataMap = 0x200,
        SSAOMap = 0x400,
        TangentMap = 0x800,
        BitangentMap = 0x1000,
        DepthMap = 0x2000
    }

    public class TextureSet
    {
        private Material material;

        public TextureSet(Material material)
        {
            this.material = material;
        }

        public Texture this[TextureKey key]
        {
            get
            {
                ManagedHandle textureHandle = new ManagedHandle();
                Material_GetTexture(material.Handle, key, out textureHandle);
                return new Texture(textureHandle);
            }
            set
            {
                Material_SetTexture(material.Handle, key, value.Handle);
            }
        }

        [DllImport("hyperion", EntryPoint = "Material_GetTexture")]
        private static extern void Material_GetTexture(ManagedHandle material, TextureKey key, [Out] out ManagedHandle handle);

        [DllImport("hyperion", EntryPoint = "Material_SetTexture")]
        private static extern void Material_SetTexture(ManagedHandle material, TextureKey key, ManagedHandle texture);
    }

    public class Material : IDisposable
    {
        private ManagedHandle handle;

        public Material()
        {
            handle = new ManagedHandle();

            Material_Create(out handle);
        }

        public Material(ManagedHandle handle)
        {
            this.handle = handle;
            this.handle.IncRef(Material_GetTypeID());
        }

        public void Dispose()
        {
            handle.DecRef(Material_GetTypeID());
        }

        public ManagedHandle Handle
        {
            get
            {
                return handle;
            }
        }

        public uint ID
        {
            get
            {
                return handle.id;
            }
        }

        public MaterialParameter this[MaterialKey key]
        {
            get
            {
                MaterialParameter materialParameter = new MaterialParameter();
                Material_GetParameter(handle, key, out materialParameter);
                return materialParameter;
            }
            set
            {
                Material_SetParameter(handle, key, ref value);
            }
        }

        public TextureSet Textures
        {
            get
            {
                return new TextureSet(this);
            }
        }

        [DllImport("hyperion", EntryPoint = "Material_GetTypeID")]
        [return: MarshalAs(UnmanagedType.Struct, SizeConst = 4)]
        private static extern TypeID Material_GetTypeID();

        [DllImport("hyperion", EntryPoint = "Material_Create")]
        private static extern void Material_Create([Out] out ManagedHandle handle);

        [DllImport("hyperion", EntryPoint = "Material_GetParameter")]
        private static extern void Material_GetParameter(ManagedHandle material, [MarshalAs(UnmanagedType.U8)] MaterialKey key, [Out] out MaterialParameter materialParameter);

        [DllImport("hyperion", EntryPoint = "Material_SetParameter")]
        private static extern void Material_SetParameter(ManagedHandle material, [MarshalAs(UnmanagedType.U8)] MaterialKey key, [In] ref MaterialParameter parameter);
    }
}