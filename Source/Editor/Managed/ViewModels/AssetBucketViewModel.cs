using System.Collections.ObjectModel;

namespace Hyperion.Editor.ViewModels
{
    public class AssetBucketViewModel : ViewModelBase
    {
        private readonly AssetBucket _bucket;

        public AssetBucket Bucket => _bucket;
        public uint BucketIndex => _bucket.Index;
        public string Name => _bucket.Name;

        public string IconKind => AssetIconHelper.FromBucket(_bucket);

        private int _assetCount;
        public int AssetCount
        {
            get => _assetCount;
            set
            {
                if (SetProperty(ref _assetCount, value))
                {
                    OnPropertyChanged(nameof(AssetCountText));
                }
            }
        }

        public string AssetCountText => _assetCount == 1 ? "1 asset" : $"{_assetCount} assets";

        public AssetBucketViewModel(AssetBucket bucket)
        {
            _bucket = bucket;
        }
    }
}
