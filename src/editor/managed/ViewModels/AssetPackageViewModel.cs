using System;
using System.Collections.Generic;
using System.Collections.ObjectModel;
using System.Linq;

namespace Hyperion.Editor.ViewModels
{
    public class AssetPackageViewModel : ViewModelBase
    {
        private readonly AssetPackage _package;

        public AssetPackage Package => _package;
        public Name Name => _package.Name;

        public ObservableCollection<AssetPackageViewModel> Subpackages { get; } = new ObservableCollection<AssetPackageViewModel>();
        public ObservableCollection<AssetObjectViewModel> Assets { get; } = new ObservableCollection<AssetObjectViewModel>();

        private DelegateHandler _onAssetAddedHandler;
        private DelegateHandler _onAssetRemovedHandler;
        private DelegateHandler _onSubpackageAddedHandler;
        private DelegateHandler _onSubpackageRemovedHandler;

        public AssetPackageViewModel(AssetPackage package)
        {
            _package = package;

            foreach (AssetObject asset in package.Assets)
            {
                Assets.Add(new AssetObjectViewModel(asset, this));
            }

            _onAssetAddedHandler = package.GetOnAssetObjectAddedDelegate().Bind((AssetObject asset, bool isDirect) =>
            {
                if (isDirect)
                    Assets.Add(new AssetObjectViewModel(asset, this));
            });

            _onAssetRemovedHandler = package.GetOnAssetObjectRemovedDelegate().Bind((AssetObject asset, bool isDirect) =>
            {
                if (isDirect)
                {
                    AssetObjectViewModel? assetViewModel = Assets.FirstOrDefault(avm => avm.Asset == asset);
                    if (assetViewModel != null)
                    {
                        Assets.Remove(assetViewModel);
                    }
                }
            });

            _onSubpackageAddedHandler = package.GetOnSubpackageAddedDelegate().Bind((AssetPackage subpackage) =>
            {
                Subpackages.Add(new AssetPackageViewModel(subpackage));
            });

            _onSubpackageRemovedHandler = package.GetOnSubpackageRemovedDelegate().Bind((AssetPackage subpackage) =>
            {
                AssetPackageViewModel? subpackageViewModel = Subpackages.FirstOrDefault(spvm => spvm.Package == subpackage);
                if (subpackageViewModel != null)
                {
                    Subpackages.Remove(subpackageViewModel);
                }
            });

            foreach (AssetPackage subpackage in package.Subpackages)
            {
                if (subpackage.Hidden)
                    continue;
                    
                Subpackages.Add(new AssetPackageViewModel(subpackage));
            }
        }

        public IReadOnlyList<AssetPackageViewModel> GetOrderedSubpackages()
        {
            return Subpackages.OrderBy(child => child.Name.ToString(), StringComparer.OrdinalIgnoreCase).ToList();
        }
    }
}
