using System;
using System.Collections.Generic;
using System.Collections.ObjectModel;
using System.IO;
using System.Linq;
using System.Threading;
using System.Threading.Tasks;
using Avalonia.Threading;
using Hyperion;

namespace Hyperion.Editor.ViewModels
{
    public class ContentBrowserViewModel : ViewModelBase, IDisposable
    {
        private readonly EditorSubsystem _editorSubsystem;

        public ObservableCollection<AssetPackageViewModel> Packages { get; } = new ObservableCollection<AssetPackageViewModel>();
        public ObservableCollection<AssetObjectViewModel> Assets { get; } = new ObservableCollection<AssetObjectViewModel>();
        
        private AssetPackageViewModel? _currentPackage;
        public AssetPackageViewModel? CurrentPackage
        {
            get => _currentPackage;
            set
            {
                _editorSubsystem.SetSelectedPackage(value?.Package ?? null);
            }
        }

        private DelegateHandler _onSelectedPackageChangedHandler;
        private DelegateHandler _onPackageAddedHandler;
        private DelegateHandler _onPackageRemovedHandler;

        public ContentBrowserViewModel(EditorSubsystem editorSubsystem)
        {
            _editorSubsystem = editorSubsystem ?? throw new ArgumentNullException(nameof(editorSubsystem));
        }

        public void LoadPackages()
        {
            Dispatcher.UIThread.CheckAccess();

            Packages.Clear();
            Assets.Clear();

            AssetManager mgr = AssetManager.Instance;
            AssetRegistry registry = mgr.AssetRegistry;

            foreach (AssetPackage pkg in registry.Packages)
            {
                if (pkg.Hidden)
                    continue;

                Packages.Add(new AssetPackageViewModel(pkg));
            }

            OnPropertyChanged(nameof(Packages));

            _onSelectedPackageChangedHandler = _editorSubsystem.GetOnSelectedPackageChangedDelegate().Bind((AssetPackage? package) =>
            {
                Dispatcher.UIThread.Post(() =>
                {
                    Assets.Clear();
                    
                    if (package != null)
                    {
                        foreach (AssetObject asset in package.Assets)
                        {
                            Assets.Add(new AssetObjectViewModel(asset));
                        }

                        _currentPackage = new AssetPackageViewModel(package);
                    }
                    else
                    {
                        _currentPackage = null;
                    }

                    OnPropertyChanged(nameof(Assets));
                    OnPropertyChanged(nameof(CurrentPackage));
                });
            });

            _onPackageAddedHandler = registry.GetOnPackageAddedDelegate().Bind((AssetPackage package) =>
            {
                Dispatcher.UIThread.Post(() =>
                {
                    if (!package.Hidden)
                    {
                        Packages.Add(new AssetPackageViewModel(package));

                        OnPropertyChanged(nameof(Packages));
                    }
                });
            });

            _onPackageRemovedHandler = registry.GetOnPackageRemovedDelegate().Bind((AssetPackage package) =>
            {
                Dispatcher.UIThread.Post(() =>
                {
                    AssetPackageViewModel? packageViewModel = Packages.FirstOrDefault(pvm => pvm.Package == package);

                    if (packageViewModel != null)
                    {
                        Packages.Remove(packageViewModel);

                        OnPropertyChanged(nameof(Packages));
                    }
                });
            });
        }

        public void Dispose()
        {
            _onSelectedPackageChangedHandler?.Dispose();
            _onPackageAddedHandler?.Dispose();
            _onPackageRemovedHandler?.Dispose();
        }
    }
}
