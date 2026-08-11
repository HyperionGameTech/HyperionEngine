using System.Runtime.InteropServices;

namespace Hyperion
{
    [ClassBinding(Name = "AssetBucket")]
    [StructLayout(LayoutKind.Explicit, Size = 4, Pack = 4)]
    public struct AssetBucket
    {
        public static readonly uint InvalidIndex = 0;

        [FieldOffset(0)]
        public uint Value;

        public AssetBucket()
        {
            Value = InvalidIndex;
        }

        public AssetBucket(uint index)
        {
            Value = index;
        }

        public uint Index => Value;
        public bool Valid => Value != InvalidIndex;
        public string Name => GetAssetBucketName(Value);

        // !!! Ensure this is kept up to date with AssetBucket.hpp !!!
        public static readonly AssetBucket None              = new(0);
        public static readonly AssetBucket Meshes            = new(1);
        public static readonly AssetBucket Textures          = new(2);
        public static readonly AssetBucket Materials         = new(3);
        public static readonly AssetBucket InstancedMeshData = new(4);
        public static readonly AssetBucket Animations        = new(5);
        public static readonly AssetBucket AnimationTracks   = new(6);
        public static readonly AssetBucket Skeletons         = new(7);
        public static readonly AssetBucket Worlds            = new(8);
        public static readonly AssetBucket Scenes            = new(9);
        public static readonly AssetBucket Shaders           = new(10);
        public static readonly AssetBucket ShaderBundles     = new(11);
        public static readonly AssetBucket FontAtlases       = new(12);
        public static readonly AssetBucket PhysicsShapes     = new(13);
        public static readonly AssetBucket Scripts           = new(14);
        public static readonly AssetBucket RawData           = new(15);
        public static readonly AssetBucket Prefabs           = new(16);
        public static readonly AssetBucket Sounds            = new(17);

        public static readonly uint MaxAssetBuckets = 18;

        public static readonly AssetBucket[] AllBuckets =
        [
            Meshes, Textures, Materials, InstancedMeshData,
            Animations, AnimationTracks, Skeletons, Worlds, Scenes,
            Shaders, ShaderBundles, FontAtlases, PhysicsShapes,
            Scripts, RawData, Prefabs, Sounds
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
