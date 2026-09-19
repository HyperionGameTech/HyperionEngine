using System;
using System.Collections.Generic;
using System.Runtime.InteropServices;

namespace Hyperion
{
    [ClassBinding(Name = "BlendModeFactor")]
    public enum BlendModeFactor
    {
        None = 0,

        One,
        Zero,
        SrcColor,
        SrcAlpha,
        DstColor,
        DstAlpha,
        OneMinusSrcColor,
        OneMinusSrcAlpha,
        OneMinusDstColor,
        OneMinusDstAlpha,

        Max
    }

    [ClassBinding(Name = "BlendFunction")]
    public struct BlendFunction
    {
        private uint value;

        public BlendFunction(BlendModeFactor srcColor, BlendModeFactor dstColor)
        {
            value = ((uint)srcColor << 0) | ((uint)dstColor << 4) | ((uint)srcColor << 8) | ((uint)dstColor << 12);
        }

        public BlendFunction(BlendModeFactor srcColor, BlendModeFactor dstColor, BlendModeFactor srcAlpha, BlendModeFactor dstAlpha)
        {
            value = ((uint)srcColor << 0) | ((uint)dstColor << 4) | ((uint)srcAlpha << 8) | ((uint)dstAlpha << 12);
        }

        public BlendModeFactor SrcColor
        {
            get => (BlendModeFactor)(value & 0xF);
            set => this.value = (this.value & ~0xFu) | ((uint)value & 0xF);
        }

        public BlendModeFactor DstColor
        {
            get => (BlendModeFactor)((value >> 4) & 0xF);
            set => this.value = (this.value & ~(0xFu << 4)) | (((uint)value & 0xF) << 4);
        }

        public BlendModeFactor SrcAlpha
        {
            get => (BlendModeFactor)((value >> 8) & 0xF);
            set => this.value = (this.value & ~(0xFu << 8)) | (((uint)value & 0xF) << 8);
        }

        public BlendModeFactor DstAlpha
        {
            get => (BlendModeFactor)((value >> 12) & 0xF);
            set => this.value = (this.value & ~(0xFu << 12)) | (((uint)value & 0xF) << 12);
        }

        public static BlendFunction None()
        {
            return new BlendFunction(BlendModeFactor.None, BlendModeFactor.None);
        }

        public static BlendFunction Default()
        {
            return new BlendFunction(BlendModeFactor.One, BlendModeFactor.Zero);
        }

        public static BlendFunction AlphaBlending()
        {
            return new BlendFunction(BlendModeFactor.SrcAlpha, BlendModeFactor.OneMinusSrcAlpha, BlendModeFactor.One, BlendModeFactor.Zero);
        }

        public static BlendFunction PremultipliedAlpha()
        {
            return new BlendFunction(BlendModeFactor.One, BlendModeFactor.OneMinusSrcAlpha, BlendModeFactor.One, BlendModeFactor.Zero);
        }

        public static BlendFunction Additive()
        {
            return new BlendFunction(BlendModeFactor.One, BlendModeFactor.One);
        }

        public bool Equals(BlendFunction other) => value == other.value;

        public override bool Equals(object? obj) => obj is BlendFunction other && Equals(other);

        public override int GetHashCode() => (int)value;

        public static bool operator ==(BlendFunction a, BlendFunction b) => a.value == b.value;

        public static bool operator !=(BlendFunction a, BlendFunction b) => a.value != b.value;
    }

    [ClassBinding(Name = "RenderBucket")]
    public enum RenderBucket : byte
    {
        Opaque = 0,
        Lightmapped,
        Translucent,
        Sky,
        Debug
    }

    [ClassBinding(Name = "FillMode")]
    public enum FillMode : byte
    {
        Fill = 0,
        Line
    }

    [ClassBinding(Name = "FaceCullMode")]
    public enum FaceCullMode : byte
    {
        None = 0,
        Back,
        Front
    }

    [ClassBinding(Name = "StencilOp")]
    public enum StencilOp : byte
    {
        Keep = 0,
        Zero,
        Replace,
        Increment,
        Decrement
    }

    [ClassBinding(Name = "StencilCompareOp")]
    public enum StencilCompareOp : byte
    {
        Always = 0,
        Never,
        Equal,
        NotEqual
    }

    [ClassBinding(Name = "DepthCompareOp")]
    public enum DepthCompareOp : byte
    {
        Less = 0,
        LessOrEqual,
        Greater,
        GreaterOrEqual,
        Equal,
        NotEqual,
        Always,
        Never
    }

    [ClassBinding(Name = "MaterialAttributeFlags")]
    [Flags]
    public enum MaterialAttributeFlags : byte
    {
        None = 0x0,
        DepthWrite = 0x1,
        DepthTest = 0x2,
        DepthBias = 0x4,
        DepthClamp = 0x8,
        StencilTest = 0x10,
        AlphaDiscard = 0x20
    }

    [ClassBinding(Name = "StencilFunction")]
    [StructLayout(LayoutKind.Sequential)]
    public struct StencilFunction
    {
        public StencilOp PassOp;
        public StencilOp FailOp;
        public StencilOp DepthFailOp;
        public StencilCompareOp CompareOp;

        public StencilFunction()
        {
            PassOp = StencilOp.Replace;
            FailOp = StencilOp.Keep;
            DepthFailOp = StencilOp.Keep;
            CompareOp = StencilCompareOp.Always;
        }
    }

    [ClassBinding(Name = "ShaderPropertySet")]
    [StructLayout(LayoutKind.Sequential)]
    public unsafe struct ShaderPropertySet
    {
        public const int NumChunks = 8;
        public const int ChunkSizeBits = 32;
        public const int MaxProperties = NumChunks * ChunkSizeBits;

        public fixed uint Chunks[NumChunks];

        public bool Test(uint propertyId)
        {
            if (propertyId >= MaxProperties)
            {
                return false;
            }

            return (Chunks[propertyId / ChunkSizeBits] & (1u << (int)(propertyId % ChunkSizeBits))) != 0;
        }

        public void Set(uint propertyId, bool enabled)
        {
            if (propertyId >= MaxProperties)
            {
                return;
            }

            uint mask = 1u << (int)(propertyId % ChunkSizeBits);

            if (enabled)
            {
                Chunks[propertyId / ChunkSizeBits] |= mask;
            }
            else
            {
                Chunks[propertyId / ChunkSizeBits] &= ~mask;
            }
        }

        public List<uint> GetSetPropertyIds()
        {
            var propertyIds = new List<uint>();

            for (uint chunkIndex = 0; chunkIndex < NumChunks; chunkIndex++)
            {
                uint chunk = Chunks[chunkIndex];

                if (chunk == 0)
                {
                    continue;
                }

                for (int bit = 0; bit < ChunkSizeBits; bit++)
                {
                    if ((chunk & (1u << bit)) != 0)
                    {
                        propertyIds.Add(chunkIndex * ChunkSizeBits + (uint)bit);
                    }
                }
            }

            return propertyIds;
        }
    }

    [ClassBinding(Name = "MaterialAttributes")]
    [StructLayout(LayoutKind.Sequential)]
    public struct MaterialAttributes
    {
        public Name ShaderName;
        public ShaderPropertySet ShaderProperties;
        public RenderBucket Bucket;
        public FillMode FillMode;
        public BlendFunction BlendFunction;
        public FaceCullMode CullFaces;
        public MaterialAttributeFlags Flags;
        public StencilFunction StencilFunction;
        public DepthCompareOp DepthCompareOp;
        public byte StencilReference;
        private ushort padding;
        public int DepthBias;
        public float DepthBiasSlope;

        public MaterialAttributes()
        {
            this.ShaderName = default;
            this.ShaderProperties = default;
            this.Bucket = RenderBucket.Opaque;
            this.FillMode = FillMode.Fill;
            this.BlendFunction = BlendFunction.None();
            this.CullFaces = FaceCullMode.Back;
            this.Flags = MaterialAttributeFlags.DepthWrite | MaterialAttributeFlags.DepthTest;
            this.StencilFunction = new StencilFunction();
            this.DepthCompareOp = DepthCompareOp.Less;
            this.StencilReference = 0;
            this.padding = 0;
            this.DepthBias = 0;
            this.DepthBiasSlope = 0.0f;
        }
    }
}