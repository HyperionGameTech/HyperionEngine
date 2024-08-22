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

    [HypClassBinding(Name="VertexAttributeSet")]
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

    [HypClassBinding(Name="Vertex")]
    [StructLayout(LayoutKind.Explicit, Size = 128, Pack = 16)]
    public unsafe struct Vertex
    {
        [FieldOffset(0)]
        Vec3f position;

        [FieldOffset(16)]
        Vec3f normal;

        [FieldOffset(32)]
        Vec3f tangent;

        [FieldOffset(48)]
        Vec3f bitangent;

        [FieldOffset(64)]
        Vec2f texCoord0;

        [FieldOffset(72)]
        Vec2f texCoord1;

        [FieldOffset(80)]
        unsafe fixed float boneWeights[4];

        [FieldOffset(96)]
        unsafe fixed int boneIndices[4];

        [FieldOffset(112)]
        byte numBoneIndices;

        [FieldOffset(113)]
        byte numBoneWeights;

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
                boneIndices[i] = 0;
            }

            numBoneIndices = 0;
            numBoneWeights = 0;
        }

        public Vertex(Vec3f position) : this()
        {
            this.position = position;
        }

        public Vertex(Vec3f position, Vec2f texCoord, Vec3f normal) : this(position)
        {
            this.normal = normal;
            texCoord0 = texCoord;
        }

        public Vec3f Position
        {
            get
            {
                return position;
            }
            set
            {
                position = value;
            }
        }

        public Vec3f Normal
        {
            get
            {
                return normal;
            }
            set
            {
                normal = value;
            }
        }

        public Vec3f Tangent
        {
            get
            {
                return tangent;
            }
            set
            {
                tangent = value;
            }
        }

        public Vec3f Bitangent
        {
            get
            {
                return bitangent;
            }
            set
            {
                bitangent = value;
            }
        }

        public Vec2f TexCoord0
        {
            get
            {
                return texCoord0;
            }
            set
            {
                texCoord0 = value;
            }
        }

        public Vec2f TexCoord1
        {
            get
            {
                return texCoord1;
            }
            set
            {
                texCoord1 = value;
            }
        }

        public float[] BoneWeights
        {
            get
            {
                float[] weights = new float[numBoneWeights];

                for (int i = 0; i < numBoneWeights; i++)
                {
                    weights[i] = boneWeights[i];
                }

                return weights;
            }
            set
            {
                numBoneWeights = (byte)value.Length;

                for (int i = 0; i < value.Length; i++)
                {
                    boneWeights[i] = value[i];
                }

                for (int i = value.Length; i < 4; i++)
                {
                    boneWeights[i] = 0;
                }

                numBoneWeights = (byte)value.Length;
            }
        }

        public int[] BoneIndices
        {
            get
            {
                int[] indices = new int[numBoneIndices];

                for (int i = 0; i < numBoneIndices; i++)
                {
                    indices[i] = boneIndices[i];
                }

                return indices;
            }
            set
            {
                numBoneIndices = (byte)value.Length;

                for (int i = 0; i < value.Length; i++)
                {
                    boneIndices[i] = value[i];
                }

                for (int i = value.Length; i < 4; i++)
                {
                    boneIndices[i] = 0;
                }

                numBoneIndices = (byte)value.Length;
            }
        }

        public int NumBoneWeights
        {
            get
            {
                return (int)numBoneWeights;
            }
        }

        public int NumBoneIndices
        {
            get
            {
                return (int)numBoneIndices;
            }
        }
    }
}
