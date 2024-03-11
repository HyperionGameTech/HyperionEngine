using System;
using System.Runtime.InteropServices;

namespace Hyperion
{
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

    [StructLayout(LayoutKind.Explicit, Size = 128)]
    public struct Vertex
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
        float boneWeights0;
        [FieldOffset(84)]
        float boneWeights1;
        [FieldOffset(88)]
        float boneWeights2;
        [FieldOffset(92)]
        float boneWeights3;
        [FieldOffset(96)]
        int boneIndices0;
        [FieldOffset(100)]
        int boneIndices1;
        [FieldOffset(104)]
        int boneIndices2;
        [FieldOffset(108)]
        int boneIndices3;
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
            boneWeights0 = 0;
            boneWeights1 = 0;
            boneWeights2 = 0;
            boneWeights3 = 0;
            boneIndices0 = 0;
            boneIndices1 = 0;
            boneIndices2 = 0;
            boneIndices3 = 0;
            numBoneIndices = 0;
            numBoneWeights = 0;
        }

        public Vertex(Vec3f position)
        {
            this.position = position;
            normal = new Vec3f();
            tangent = new Vec3f();
            bitangent = new Vec3f();
            texCoord0 = new Vec2f();
            texCoord1 = new Vec2f();
            boneWeights0 = 0;
            boneWeights1 = 0;
            boneWeights2 = 0;
            boneWeights3 = 0;
            boneIndices0 = 0;
            boneIndices1 = 0;
            boneIndices2 = 0;
            boneIndices3 = 0;
            numBoneIndices = 0;
            numBoneWeights = 0;
        }

        public Vertex(Vec3f position, Vec2f texCoord, Vec3f normal)
        {
            this.position = position;
            this.normal = normal;
            tangent = new Vec3f();
            bitangent = new Vec3f();
            texCoord0 = texCoord;
            texCoord1 = new Vec2f();
            boneWeights0 = 0;
            boneWeights1 = 0;
            boneWeights2 = 0;
            boneWeights3 = 0;
            boneIndices0 = 0;
            boneIndices1 = 0;
            boneIndices2 = 0;
            boneIndices3 = 0;
            numBoneIndices = 0;
            numBoneWeights = 0;
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
                return new float[] { boneWeights0, boneWeights1, boneWeights2, boneWeights3 };
            }
            set
            {
                numBoneWeights = (byte)value.Length;

                if (value.Length > 0)
                {
                    boneWeights0 = value[0];
                }

                if (value.Length > 1)
                {
                    boneWeights1 = value[1];
                }

                if (value.Length > 2)
                {
                    boneWeights2 = value[2];
                }

                if (value.Length > 3)
                {
                    boneWeights3 = value[3];
                }
            }
        }

        public int[] BoneIndices
        {
            get
            {
                return new int[] { boneIndices0, boneIndices1, boneIndices2, boneIndices3 };
            }
            set
            {
                numBoneIndices = (byte)value.Length;

                if (value.Length > 0)
                {
                    boneIndices0 = value[0];
                }

                if (value.Length > 1)
                {
                    boneIndices1 = value[1];
                }

                if (value.Length > 2)
                {
                    boneIndices2 = value[2];
                }

                if (value.Length > 3)
                {
                    boneIndices3 = value[3];
                }
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
