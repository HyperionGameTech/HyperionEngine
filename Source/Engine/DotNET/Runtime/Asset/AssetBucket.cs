using System.Runtime.InteropServices;

namespace Hyperion
{
    [ClassBinding(Name = "AssetBucket")]
    [StructLayout(LayoutKind.Explicit, Size = 4, Pack = 4)]
    public struct AssetBucket
    {
        public static readonly uint InvalidBucket = 0;

        [FieldOffset(0)]
        public uint Value;

        public AssetBucket()
        {
            Value = InvalidBucket;
        }

        public AssetBucket(uint index)
        {
            Value = index;
        }

        public uint Index => Value;
        public bool Valid => Value != InvalidBucket;
        public string Name => GetAssetBucketName(Value);

        // !!! Ensure this is kept up to date with AssetBucket.hpp !!!
        public static readonly AssetBucket None                 = new(0);
        public static readonly AssetBucket Meshes               = new(1);
        public static readonly AssetBucket Materials            = new(2);
        public static readonly AssetBucket Textures             = new(3);
        public static readonly AssetBucket Lights               = new(4);
        public static readonly AssetBucket InstancedMeshData    = new(5);
        public static readonly AssetBucket Animations           = new(6);
        public static readonly AssetBucket AnimationTracks      = new(7);
        public static readonly AssetBucket Skeletons            = new(8);
        public static readonly AssetBucket Worlds               = new(9);
        public static readonly AssetBucket Scenes               = new(10);
        public static readonly AssetBucket Nodes                = new(11);
        public static readonly AssetBucket Entities             = new(12);
        public static readonly AssetBucket Bones                = new(13);
        public static readonly AssetBucket EnvProbes            = new(14);
        public static readonly AssetBucket LightmapVolumes      = new(15);
        public static readonly AssetBucket Shaders              = new(16);
        public static readonly AssetBucket ShaderBundles        = new(17);
        public static readonly AssetBucket FontAtlases          = new(18);
        public static readonly AssetBucket PhysicsShapes        = new(19);
        public static readonly AssetBucket Scripts              = new(20);
        public static readonly AssetBucket Sprites              = new(21);
        public static readonly AssetBucket RawData              = new(22);

        public static readonly uint MaxAssetBuckets = 23;

        public static readonly AssetBucket[] AllBuckets =
        [
            Meshes, Materials, Textures, Lights,
            InstancedMeshData, Animations, AnimationTracks, Skeletons, Worlds,
            Scenes, Nodes, Entities, Bones, EnvProbes,
            LightmapVolumes, Shaders, ShaderBundles, FontAtlases,
            PhysicsShapes, Scripts, Sprites, RawData
        ];

        public static string GetAssetBucketName(uint bucketIndex)
        {
            IntPtr ptr = AssetRegistry_GetBucketName(bucketIndex);
            return Marshal.PtrToStringAnsi(ptr) ?? string.Empty;
        }

        [DllImport("hyperion", EntryPoint = "AssetRegistry_GetBucketName")]
        private static extern IntPtr AssetRegistry_GetBucketName(uint bucketIndex);
    }
}
