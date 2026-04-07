namespace Hyperion.Editor.ViewModels
{
    public class AssetObjectViewModel : ViewModelBase
    {
        private readonly AssetDesc _assetDesc;
        private readonly AssetPackageViewModel? _package;

        public AssetDesc AssetDesc => _assetDesc;
        public AssetPackageViewModel? Package => _package;

        public string DisplayName => _assetDesc.Name.ToString();

        public AssetObjectViewModel(AssetDesc assetDesc, AssetPackageViewModel? package = null)
        {
            _assetDesc = assetDesc;
            _package = package;
        }
    }
}
