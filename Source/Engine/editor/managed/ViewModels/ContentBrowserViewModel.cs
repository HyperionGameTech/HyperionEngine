using System;
using System.Collections.Generic;
using System.Collections.ObjectModel;
using System.IO;
using System.Linq;
using System.Threading;
using System.Threading.Tasks;
using System.Windows.Input;
using Avalonia.Threading;
using Hyperion;
using Hyperion.Editor.Commands;

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

        private DelegateHandler? _onSelectedPackageChangedHandler;
        private DelegateHandler? _onPackageAddedHandler;
        private DelegateHandler? _onPackageRemovedHandler;

        public ICommand ImportCommand { get; }

        public ContentBrowserViewModel(EditorSubsystem editorSubsystem)
        {
            _editorSubsystem = editorSubsystem ?? throw new ArgumentNullException(nameof(editorSubsystem));

            ImportCommand = new EditorCommand("ImportContent");
        }

        public void LoadPackages()
        {
            Dispatcher.UIThread.VerifyAccess();

            Logger.Log(LogLevel.Verbose, "Loading content browser packages...");

            Packages.Clear();
            Assets.Clear();

            AssetManager mgr = AssetManager.Instance;
            AssetRegistry registry = mgr.AssetRegistry;

            foreach (AssetPackage pkg in registry.Packages)
            {
                Logger.Log(LogLevel.Verbose, "Found package: {0}", pkg.Name);

                if (pkg.Hidden)
                    continue;

                Packages.Add(new AssetPackageViewModel(pkg));
            }

            OnPropertyChanged(nameof(Packages));

            _onSelectedPackageChangedHandler = _editorSubsystem.GetOnSelectedPackageChangedDelegate().Bind((AssetPackage? package) =>
            {
                Logger.Log(LogLevel.Verbose, "Selected package changed: {0}", package?.Name ?? "null");
                
                Dispatcher.UIThread.Post(() =>
                {
                    Assets.Clear();
                    
                    if (package != null)
                    {
                        foreach (AssetDesc assetDesc in package.AssetDescs)
                        {
                            Assets.Add(new AssetObjectViewModel(assetDesc));
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
                Logger.Log(LogLevel.Verbose, "Package added: {0}", package.Name);

                if (!package.Hidden && package.GetParentPackage() == null)
                {
                    WeakReference<AssetPackage> weakPackage = new(package);

                    Dispatcher.UIThread.Post(() =>
                    {
                        AssetPackage? package = null;
                        if (weakPackage.TryGetTarget(out package))
                        {
                            AssetPackageViewModel? packageViewModel = Packages.FirstOrDefault(pvm => pvm.Package.Id == package.Id);

                            if (packageViewModel != null)
                                return; // already exists

                            Packages.Add(new AssetPackageViewModel(package));

                            OnPropertyChanged(nameof(Packages));
                        }
                    });
                }
            });

            _onPackageRemovedHandler = registry.GetOnPackageRemovedDelegate().Bind((AssetPackage package) =>
            {
                Logger.Log(LogLevel.Verbose, "Package removed: {0}", package.Name);

                ObjIdBase removedPackageId = package.Id;

                Dispatcher.UIThread.Post(() =>
                {
                    AssetPackageViewModel? packageViewModel = Packages.FirstOrDefault(pvm => pvm.Package.Id == removedPackageId);

                    if (packageViewModel == null)
                        return;

                    Packages.Remove(packageViewModel);

                    OnPropertyChanged(nameof(Packages));
                });
            });
        }

        public void Dispose()
        {
            _onSelectedPackageChangedHandler?.Remove();
            _onSelectedPackageChangedHandler?.Dispose();

            _onSelectedPackageChangedHandler?.Remove();
            _onPackageAddedHandler?.Dispose();

            _onPackageRemovedHandler?.Remove();
            _onPackageRemovedHandler?.Dispose();
        }
    }
}
