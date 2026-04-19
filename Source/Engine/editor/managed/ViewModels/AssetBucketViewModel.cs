using System.Collections.ObjectModel;

namespace Hyperion.Editor.ViewModels
{
    public class AssetBucketViewModel : ViewModelBase
    {
        private readonly AssetBucket _bucket;

        public AssetBucket Bucket => _bucket;
        public uint BucketIndex => _bucket.Index;
        public string Name => _bucket.Name;

        public AssetBucketViewModel(AssetBucket bucket)
        {
            _bucket = bucket;
        }
    }
}
