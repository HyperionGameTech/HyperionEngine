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
    public struct MaterialParameters
    {
        const byte FlagBit_NormalMapFlipY = 0x1;
        const int FlagShift_RoughnessChannel = 1;
        const int FlagShift_MetalnessChannel = 3;
        const int FlagShift_AmbientOcclusionChannel = 5;
        const byte FlagMask_Channel = 0x3;
        const byte FlagBit_ParallaxInverseHeight = 0x80;

        public Vec4f albedo = Vec4f.One;
        
        public float metalness = 0.0f;
        public float roughness = 1.0f;
        
        public float alphaThreshold = 0.2f;
        public float parallaxHeightScale = 0.02f;
        public float transmission = 0.0f;
        public float ior = 1.5f;
        
        public Color emissiveColor;

        float emissiveIntensity = 0.0f;

        Vec4f userParams = Vec4f.Zero;
        
        Vec2f uvScale = Vec2f.One;

        [MarshalAs(UnmanagedType.I1)]
        bool unlit = false;

        byte flags = 0;

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
