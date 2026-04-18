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
        public static readonly AssetBucket MaterialDefinitions  = new(2);
        public static readonly AssetBucket MaterialInstances    = new(3);
        public static readonly AssetBucket Textures             = new(4);
        public static readonly AssetBucket Lights               = new(5);
        public static readonly AssetBucket InstancedMeshData    = new(6);
        public static readonly AssetBucket Animations           = new(7);
        public static readonly AssetBucket AnimationTracks      = new(8);
        public static readonly AssetBucket Skeletons            = new(9);
        public static readonly AssetBucket Worlds               = new(10);
        public static readonly AssetBucket Scenes               = new(11);
        public static readonly AssetBucket Nodes                = new(12);
        public static readonly AssetBucket Entities             = new(13);
        public static readonly AssetBucket Bones                = new(14);
        public static readonly AssetBucket EnvProbes            = new(15);
        public static readonly AssetBucket LightmapVolumes      = new(16);
        public static readonly AssetBucket Shaders              = new(17);
        public static readonly AssetBucket ShaderBundles        = new(18);
        public static readonly AssetBucket FontAtlases          = new(19);

        public static readonly uint MaxAssetBuckets = 19;

        public static readonly AssetBucket[] AllBuckets =
        [
            Meshes, MaterialDefinitions, MaterialInstances, Textures, Lights,
            InstancedMeshData, Animations, AnimationTracks, Skeletons, Worlds,
            Scenes, Nodes, Entities, Bones, EnvProbes,
            LightmapVolumes, Shaders, ShaderBundles, FontAtlases
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
