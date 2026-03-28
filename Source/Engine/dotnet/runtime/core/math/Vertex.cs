using System;
using System.Runtime.InteropServices;

namespace Hyperion
{
    [Flags]
    public enum VertexAttributeType : ulong
    {
        Undefined = 0x0,
        Position = 0x1,
        Normal = 0x2,
        TexCoord0 = 0x4,
        TexCoord1 = 0x8,
        Tangent = 0x10,
        Bitangent = 0x20,
        BoneIndices = 0x40,
        BoneWeights = 0x80
    }

    [ClassBinding(Name="VertexAttributeSet")]
    [StructLayout(LayoutKind.Explicit, Size = 8)]
    public struct VertexAttributeSet
    {
        [FieldOffset(0)]
        private ulong flagMask;

        public VertexAttributeSet()
        {
            flagMask = 0;
        }

        public VertexAttributeSet(ulong flagMask)
        {
            this.flagMask = flagMask;
        }

        public VertexAttributeSet(VertexAttributeType[] types)
        {
            flagMask = 0;

            foreach (VertexAttributeType type in types)
            {
                flagMask |= (ulong)type;
            }
        }

        public ulong FlagMask
        {
            get
            {
                return flagMask;
            }
            set
            {
                flagMask = value;
            }
        }
    }

    [ClassBinding(Name="Vertex")]
    [StructLayout(LayoutKind.Sequential, Pack = 16)]
    public unsafe struct Vertex
    {
        public Vec3f position;
        public Vec3f normal;
        public Vec3f tangent;
        public Vec3f bitangent;
        public Vec2f texCoord0;
        public Vec2f texCoord1;
        public fixed float boneWeights[4];
        public uint boneIndices;

        public Vertex()
        {
            position = new Vec3f();
            normal = new Vec3f();
            tangent = new Vec3f();
            bitangent = new Vec3f();
            texCoord0 = new Vec2f();
            texCoord1 = new Vec2f();
            
            for (int i = 0; i < 4; i++)
            {
                boneWeights[i] = 0;
            }
            
            boneIndices = uint.MaxValue; // each byte is 0xFF (meaning invalid bone index)
        }

        public Vertex(Vec3f position) : this()
        {
            this.position = position;
        }

        public Vertex(Vec3f position, Vec2f texCoord, Vec3f normal) : this(position)
        {
            this.normal = normal;
            this.texCoord0 = texCoord;
        }
    }
}
