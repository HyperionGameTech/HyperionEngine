using System.Runtime.InteropServices;

namespace Hyperion
{
    [ClassBinding(Name = "AssetPath")]
    [StructLayout(LayoutKind.Sequential)]
    public struct AssetPath
    {
        public static readonly AssetPath Invalid = new AssetPath();

        private Name assetName;
        private uint bucketIndex;

        public AssetPath()
        {
            assetName = new Name();
            bucketIndex = AssetBucket.InvalidBucket;
        }

        public bool Valid => assetName.Valid && bucketIndex != AssetBucket.InvalidBucket;

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