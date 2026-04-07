using Avalonia.Threading;
using System;
using System.Collections.Generic;
using System.Collections.ObjectModel;
using System.Diagnostics;
using System.Linq;

namespace Hyperion.Editor.ViewModels
{
    public class AssetPackageViewModel : ViewModelBase
    {
        private readonly AssetPackage _package;

        public AssetPackage Package => _package;
        public Name Name => _package.Name;

        public ObservableCollection<AssetPackageViewModel> Subpackages { get; } = new();
        public ObservableCollection<AssetObjectViewModel> Assets { get; } = new();

        private DelegateHandler _onAssetAddedHandler;
        private DelegateHandler _onAssetRemovedHandler;
        private DelegateHandler _onSubpackageAddedHandler;
        private DelegateHandler _onSubpackageRemovedHandler;

        public AssetPackageViewModel(AssetPackage package)
        {
            _package = package;

            foreach (AssetDesc assetDesc in package.AssetDescs)
            {
                Assets.Add(new AssetObjectViewModel(assetDesc, this));
            }

            foreach (AssetPackage subpackage in package.Subpackages)
            {
                if (subpackage.Hidden)
                    continue;

                Subpackages.Add(new AssetPackageViewModel(subpackage));
            }

            OnPropertyChanged(nameof(Assets));
            OnPropertyChanged(nameof(Subpackages));

            _onAssetAddedHandler = package.GetOnAssetObjectAddedDelegate().Bind((AssetDesc assetDesc, bool isDirect, AssetPackage parentPackage) =>
            {
                Debug.Assert(assetDesc.Index != AssetDesc.InvalidIndex);

                if (isDirect)
                {
                    Dispatcher.UIThread.Post(() =>
                    {
                        AssetObjectViewModel? assetViewModel = Assets.FirstOrDefault(avm => avm.AssetDesc.Index == assetDesc.Index);

                        if (assetViewModel != null)
                            return; // already exists

                        Assets.Add(new AssetObjectViewModel(assetDesc, this));

                        OnPropertyChanged(nameof(Assets));
                    });
                }
            });

            _onAssetRemovedHandler = package.GetOnAssetObjectRemovedDelegate().Bind((Name assetName, bool isDirect, AssetPackage parentPackage) =>
            {
                if (isDirect)
                {
                    Dispatcher.UIThread.Post(() =>
                    {
                        AssetObjectViewModel? assetViewModel = Assets.FirstOrDefault(avm => avm.AssetDesc.Name == assetName);

                        if (assetViewModel == null)
                            return;

                        Assets.Remove(assetViewModel);

                        OnPropertyChanged(nameof(Assets));
                    });
                }
            });

            _onSubpackageAddedHandler = package.GetOnSubpackageAddedDelegate().Bind((AssetPackage subpackage) =>
            {
                if (!subpackage.Hidden)
                {
                    WeakReference<AssetPackage> weakPackage = new(subpackage);

                    Dispatcher.UIThread.Post(() =>
                    {
                        AssetPackage? subpackage = null;
                        if (weakPackage.TryGetTarget(out subpackage))
                        {
                            AssetPackageViewModel? packageViewModel = Subpackages.FirstOrDefault(pvm => pvm.Package.Id == subpackage.Id);

                            if (packageViewModel != null)
                                return; // already exists

                            Subpackages.Add(new AssetPackageViewModel(subpackage));

                            OnPropertyChanged(nameof(Subpackages));
                        }
                    });
                }
            });

            _onSubpackageRemovedHandler = package.GetOnSubpackageRemovedDelegate().Bind((AssetPackage subpackage) =>
            {
                ObjIdBase removedPackageId = subpackage.Id;

                Dispatcher.UIThread.Post(() =>
                {
                    AssetPackageViewModel? packageViewModel = Subpackages.FirstOrDefault(pvm => pvm.Package.Id == removedPackageId);

                    if (packageViewModel == null)
                        return;

                    Subpackages.Remove(packageViewModel);

                    OnPropertyChanged(nameof(Subpackages));
                });
            });
        }

        public IReadOnlyList<AssetPackageViewModel> GetOrderedSubpackages()
        {
            return Subpackages.OrderBy(child => child.Name.ToString(), StringComparer.OrdinalIgnoreCase).ToList();
        }

        public void Dispose()
        {
            _onAssetAddedHandler?.Remove();
            _onAssetAddedHandler?.Dispose();

            _onAssetRemovedHandler?.Remove();
            _onAssetRemovedHandler?.Dispose();

            _onSubpackageAddedHandler?.Remove();
            _onSubpackageAddedHandler?.Dispose();

            _onSubpackageRemovedHandler?.Remove();
            _onSubpackageRemovedHandler?.Dispose();
        }
    }
}
