namespace Hyperion.Editor.ViewModels
{
    public class AssetObjectViewModel : ViewModelBase
    {
        private readonly AssetDesc _assetDesc;
        private readonly AssetBucketViewModel? _bucket;

        public AssetDesc AssetDesc => _assetDesc;
        public AssetBucketViewModel? Bucket => _bucket;

        public string DisplayName => _assetDesc.Name.ToString();

        public AssetObjectViewModel(AssetDesc assetDesc, AssetBucketViewModel? bucket = null)
        {
            _assetDesc = assetDesc;
            _bucket = bucket;
        }
    }
}
