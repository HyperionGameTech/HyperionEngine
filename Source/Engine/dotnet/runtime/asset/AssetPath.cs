using System.Runtime.InteropServices;

namespace Hyperion
{
    [ClassBinding(Name = "AssetPath")]
    [StructLayout(LayoutKind.Sequential)]
    public struct AssetPath
    {
        public static readonly AssetPath Invalid = new AssetPath();

        private Name assetName;
        private uint bucketIndexAndRegistryId;

        public AssetPath()
        {
            assetName = new Name();
            bucketIndexAndRegistryId = 0;
        }

        public bool Valid => assetName.Valid
            // The bucket index is stored in the lower 24 bits, and the registry ID in the upper 8 bits.
            && (bucketIndexAndRegistryId & 0xFFFFFF) != AssetBucket.InvalidBucket;

        public override string ToString()
        {
            /// Same impl as native AssetPath::ToString().
            
            if (!Valid)
                return "<Invalid>";

            string bucketName = AssetBucket.GetAssetBucketName(bucketIndex);
            return $"{bucketName}/{assetName}";
        }
    }
}