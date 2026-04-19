using System.Runtime.InteropServices;

namespace Hyperion
{
    [ClassBinding(Name = "AssetPath")]
    [StructLayout(LayoutKind.Explicit, Size = 12, Pack = 4)]
    public struct AssetPath
    {
        public static readonly AssetPath Invalid = new AssetPath();

        [FieldOffset(0)]
        private Name assetName;
        
        [FieldOffset(8)]
        private uint bucketIndexAndRegistryId;

        public AssetPath()
        {
            assetName = new Name();
            bucketIndexAndRegistryId = 0;
        }

        public uint BucketIndex => (bucketIndexAndRegistryId >> 3);

        public bool Valid => assetName.Valid
            && BucketIndex != AssetBucket.InvalidBucket;

        public override string ToString()
        {
            /// Same impl as native AssetPath::ToString().
            
            if (!Valid)
                return "<Invalid>";

            string bucketName = AssetBucket.GetAssetBucketName(BucketIndex);
            return $"{bucketName}/{assetName}";
        }
    }
}