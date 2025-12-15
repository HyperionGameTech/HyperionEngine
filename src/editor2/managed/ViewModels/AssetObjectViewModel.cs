namespace Hyperion.Editor.ViewModels
{
    public class AssetObjectViewModel : ViewModelBase
    {
        private readonly AssetObject _asset;
        private readonly AssetPackageViewModel? _package;

        public AssetObject Asset => _asset;
        public AssetPackageViewModel? Package => _package;

        public string DisplayName => _asset.Name.ToString();

        public AssetObjectViewModel(AssetObject asset, AssetPackageViewModel? package = null)
        {
            _asset = asset;
            _package = package;
        }
    }
}
