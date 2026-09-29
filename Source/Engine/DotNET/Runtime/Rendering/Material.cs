using System;
using System.Runtime.InteropServices;

namespace Hyperion
{
    public enum MaterialTextureChannel : byte
    {
        R = 0,
        G = 1,
        B = 2,
        A = 3
    }

    [ClassBinding(Name = "MaterialParameters")]
    [StructLayout(LayoutKind.Explicit, Size = 128, Pack = 16)]
    public struct MaterialParameters
    {
        const byte FlagBit_NormalMapFlipY = 0x1;
        const int FlagShift_RoughnessChannel = 1;
        const int FlagShift_MetalnessChannel = 3;
        const int FlagShift_AmbientOcclusionChannel = 5;
        const byte FlagMask_Channel = 0x3;
        const byte FlagBit_ParallaxInverseHeight = 0x80;

        [FieldOffset(0)]
        public Color albedo = new Color(1.0f, 1.0f, 1.0f, 1.0f);

        [FieldOffset(4)]
        public float metalness = 0.0f;
        [FieldOffset(8)]
        public float roughness = 1.0f;

        [FieldOffset(12)]
        public float alphaThreshold = 0.2f;
        [FieldOffset(16)]
        public float parallaxHeightScale = 0.02f;
        [FieldOffset(20)]
        public float transmission = 0.0f;
        [FieldOffset(24)]
        public float ior = 1.5f;

        [FieldOffset(28)]
        public Color emissiveColor;

        [FieldOffset(32)]
        float emissiveIntensity = 0.0f;

        [FieldOffset(48)]
        Vec4f userParams = Vec4f.Zero;

        [FieldOffset(64)]
        public float windFrequency = 0.0f;
        [FieldOffset(68)]
        public float windTrunkFlexibility = 0.0f;
        [FieldOffset(72)]
        public float windTreeHeight = 0.0f;
        [FieldOffset(76)]
        public float windFlutter = 0.0f;

        [FieldOffset(80)]
        public float foliageNormalBlend = 0.0f;
        [FieldOffset(84)]
        public float foliageBackfaceVolume = 0.0f;

        [FieldOffset(88)]
        Vec2f uvScale = Vec2f.One;

        [FieldOffset(96)]
        [MarshalAs(UnmanagedType.I1)]
        bool unlit = false;

        [FieldOffset(97)]
        [MarshalAs(UnmanagedType.I1)]
        bool foliage = false;

        [FieldOffset(98)]
        byte flags = 0;

        [FieldOffset(100)]
        public float colorVariation = 0.0f;
        [FieldOffset(104)]
        public float groundNormalBlend = 0.0f;
        [FieldOffset(108)]
        public float baseOcclusion = 0.0f;
        [FieldOffset(112)]
        public float baseOcclusionHeight = 0.0f;

        public MaterialParameters()
        {
        }

        public bool NormalMapFlipY
        {
            readonly get => (flags & FlagBit_NormalMapFlipY) != 0;
            set => flags = value
                ? (byte)(flags | FlagBit_NormalMapFlipY)
                : (byte)(flags & ~FlagBit_NormalMapFlipY);
        }

        public MaterialTextureChannel RoughnessChannel
        {
            readonly get => (MaterialTextureChannel)((flags >> FlagShift_RoughnessChannel) & FlagMask_Channel);
            
            set => flags = (byte)((flags & ~(FlagMask_Channel << FlagShift_RoughnessChannel))
                | (((byte)value & FlagMask_Channel) << FlagShift_RoughnessChannel));
        }

        public MaterialTextureChannel MetalnessChannel
        {
            readonly get => (MaterialTextureChannel)((flags >> FlagShift_MetalnessChannel) & FlagMask_Channel);
            
            set => flags = (byte)((flags & ~(FlagMask_Channel << FlagShift_MetalnessChannel))
                | (((byte)value & FlagMask_Channel) << FlagShift_MetalnessChannel));
        }

        public MaterialTextureChannel AmbientOcclusionChannel
        {
            readonly get => (MaterialTextureChannel)((flags >> FlagShift_AmbientOcclusionChannel) & FlagMask_Channel);

            set => flags = (byte)((flags & ~(FlagMask_Channel << FlagShift_AmbientOcclusionChannel))
                | (((byte)value & FlagMask_Channel) << FlagShift_AmbientOcclusionChannel));
        }

        public bool InverseHeight
        {
            readonly get => (flags & FlagBit_ParallaxInverseHeight) != 0;
            set => flags = value
                ? (byte)(flags | FlagBit_ParallaxInverseHeight)
                : (byte)(flags & ~FlagBit_ParallaxInverseHeight);
        }
    }

    [ClassBinding(Name = "Material")]
    public class Material : AssetObject
    {
        public Material()
        {
        }
    }
}
