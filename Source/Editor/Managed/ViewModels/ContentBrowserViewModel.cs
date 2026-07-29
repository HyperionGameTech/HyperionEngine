using System;
using System.Collections.Generic;
using System.Collections.ObjectModel;
using System.IO;
using System.Linq;
using System.Threading;
using System.Threading.Tasks;
using System.Windows.Input;
using System.Diagnostics;
using Avalonia.Threading;
using Hyperion;
using Hyperion.Editor.Commands;
using Hyperion.Editor.Services;
using Hyperion.Editor.Views;

namespace Hyperion.Editor.ViewModels
{
    public enum AssetSortMode
    {
        Name,
        DateModified,
        Type,
    }

    public class ContentBrowserViewModel : ViewModelBase, IDisposable
    {
        public static ContentBrowserViewModel? Instance { get; private set; }

        private readonly EditorSubsystem _editorSubsystem;

        public ObservableCollection<AssetBucketViewModel> Buckets { get; } = new ObservableCollection<AssetBucketViewModel>();
        public ObservableCollection<AssetObjectViewModel> Assets { get; } = new ObservableCollection<AssetObjectViewModel>();

        private AssetObjectViewModel? _selectedAsset;
        public AssetObjectViewModel? SelectedAsset
        {
            get => _selectedAsset;
            set => SetProperty(ref _selectedAsset, value);
        }

        private AssetBucketViewModel? _currentBucket;
        public AssetBucketViewModel? CurrentBucket
        {
            get => _currentBucket;
            set
            {
                _editorSubsystem.SetSelectedBucket(value?.BucketIndex ?? 0);
            }
        }


        public IReadOnlyList<string> SortModeLabels { get; } = new[] { "Name", "Date Modified", "Type" };

        private AssetSortMode _sortMode = AssetSortMode.Name;
        private int _sortModeIndex = 0;

        public int SortModeIndex
        {
            get => _sortModeIndex;
            set
            {
                if (SetProperty(ref _sortModeIndex, value))
                {
                    _sortMode = (AssetSortMode)value;
                    ApplySort();
                }
            }
        }


        private DelegateHandler? _onSelectedBucketChangedHandler;
        private uint _pendingFocusBucket;
        private string? _pendingFocusNameHint;
        private bool _pendingOpenEditor;

        public ICommand ImportCommand { get; }

        public ICommand NewScriptCommand { get; }
        public ICommand NewMaterialCommand { get; }
        public ICommand NewPhysicsShapeCommand { get; }

        public ICommand DeleteAssetCommand { get; }
        public ICommand EditAssetCommand { get; }

        public ContentBrowserViewModel(EditorSubsystem editorSubsystem)
        {
            _editorSubsystem = editorSubsystem ?? throw new ArgumentNullException(nameof(editorSubsystem));

            ImportCommand = new EditorCommand("ImportContent");
            DeleteAssetCommand = new RelayCommand<AssetObjectViewModel>(asset =>
            {
                if (asset?.Bucket == null)
                {
                    return;
                }

                _editorSubsystem.ExecuteCommandByName(new Name("EditorCommandDeleteAsset"), $"{asset.Bucket.BucketIndex} {asset.AssetDesc.Name}");
            });

            EditAssetCommand = new RelayCommand<AssetObjectViewModel>(asset =>
            {
                OpenAssetEditor(asset);
            });

            NewScriptCommand = new RelayCommand(() =>
            {
                _editorSubsystem.ExecuteCommandByName(new Name("EditorCommandNewScript"));
                FocusAsset(AssetBucket.Scripts.Value, "NewScript");
            });

            NewMaterialCommand = new RelayCommand(() =>
            {
                _editorSubsystem.ExecuteCommandByName(new Name("EditorCommandNewMaterial"));
                FocusAsset(AssetBucket.Materials.Value, "NewMaterial", openEditor: true);
            });

            NewPhysicsShapeCommand = new RelayCommand(() =>
            {
                var panel = new NewPhysicsShapePanelViewModel(shape =>
                {
                    if (shape == null)
                    {
                        Logger.Log(LogLevel.Warning, "Physics shape creation cancelled.");
                        return;
                    }

                    // Task hell...
                    _ = EngineManager.PostToSimThread(() =>
                    {
                        AssetRegistry? registry = EngineManager.EditorGame?.AssetRegistry;
                        Debug.Assert(registry != null);

                        registry.PutAssetUnique(shape);

                        Dispatcher.UIThread.Post(() =>
                        {
                            FocusAsset(AssetBucket.PhysicsShapes.Value, shape.GetName().ToString());
                        });
                    });
                });

                PanelService.Instance.OpenPanel(panel);
            });

            Instance = this;
        }

        public void LoadBuckets()
        {
            Dispatcher.UIThread.VerifyAccess();

            Logger.Log(LogLevel.Verbose, "Loading content browser buckets...");

            Buckets.Clear();
            Assets.Clear();

            foreach (AssetBucket bucket in AssetBucket.AllBuckets)
            {
                Buckets.Add(new AssetBucketViewModel(bucket));
            }

            OnPropertyChanged(nameof(Buckets));

            _onSelectedBucketChangedHandler = _editorSubsystem.GetOnSelectedBucketChangedDelegate().Bind((uint bucketIndex) =>
            {
                Logger.Log(LogLevel.Verbose, "Selected bucket changed: {0}", AssetBucket.GetAssetBucketName(bucketIndex));

                Dispatcher.UIThread.Post(() => ReloadBucketAssets(bucketIndex));
            });
        }

        /// <summary>Reloads the asset list for the given bucket and resolves any pending focus/edit request. Must run on the UI thread.</summary>
        private void ReloadBucketAssets(uint bucketIndex)
        {
            Assets.Clear();
            SelectedAsset = null;

            if (bucketIndex != 0)
            {
                AssetBucketViewModel? bucketVm = Buckets.FirstOrDefault(bvm => bvm.BucketIndex == bucketIndex);

                if (bucketVm != null)
                {
                    AssetManager mgr = AssetManager.Instance;
                    AssetRegistry registry = mgr.AssetRegistry;
                    string rootPath = registry.GetRootPath();

                    foreach (AssetDesc assetDesc in registry.GetBucketAssetDescs(bucketIndex))
                    {
                        string manifestPath = Path.Combine(rootPath, bucketVm.Name, assetDesc.Name.ToString() + ".json");

                        DateTime? dateModified = null;
                        string? typeName = null; // @TODO

                        if (File.Exists(manifestPath))
                        {
                            dateModified = File.GetLastWriteTime(manifestPath);
                        }

                        Assets.Add(new AssetObjectViewModel(assetDesc, bucketVm, typeName, dateModified));
                    }

                    _currentBucket = bucketVm;
                }
                else
                {
                    _currentBucket = null;
                }
            }
            else
            {
                _currentBucket = null;
            }

            ApplySort();

            // Handle pending focus after asset creation
            if (_pendingFocusBucket == bucketIndex)
            {
                _pendingFocusBucket = 0;

                if (_pendingFocusNameHint != null)
                {
                    SelectedAsset = Assets.FirstOrDefault(a =>
                        a.DisplayName.StartsWith(_pendingFocusNameHint, StringComparison.OrdinalIgnoreCase));
                    _pendingFocusNameHint = null;
                }

                SelectedAsset ??= Assets.FirstOrDefault();

                if (_pendingOpenEditor)
                {
                    _pendingOpenEditor = false;
                    OpenAssetEditor(SelectedAsset);
                }
            }

            OnPropertyChanged(nameof(Assets));
            OnPropertyChanged(nameof(CurrentBucket));
        }

        private void ApplySort()
        {
            if (Assets.Count == 0)
                return;

            var preserved = SelectedAsset;

            List<AssetObjectViewModel> sorted = _sortMode switch
            {
                AssetSortMode.DateModified =>
                    Assets.OrderByDescending(a => a.DateModified ?? DateTime.MinValue).ToList(),
                AssetSortMode.Type =>
                    Assets.OrderBy(a => a.TypeName ?? string.Empty, StringComparer.OrdinalIgnoreCase)
                          .ThenBy(a => a.DisplayName, StringComparer.OrdinalIgnoreCase)
                          .ToList(),
                _ /* Name */ =>
                    Assets.OrderBy(a => a.DisplayName, StringComparer.OrdinalIgnoreCase).ToList(),
            };

            Assets.Clear();
            foreach (var vm in sorted)
                Assets.Add(vm);

            SelectedAsset = preserved;
        }


        public void Dispose()
        {
            _onSelectedBucketChangedHandler?.Remove();
            _onSelectedBucketChangedHandler?.Dispose();
        }

        /// <summary>Switches to the given bucket and focuses the named asset once loaded.</summary>
        public void FocusAsset(uint bucketIndex, string? nameHint = null, bool openEditor = false)
        {
            Dispatcher.UIThread.VerifyAccess();

            if (bucketIndex == 0)
                return;

            _pendingFocusBucket = bucketIndex;
            _pendingFocusNameHint = nameHint;
            _pendingOpenEditor = openEditor;

            if (_currentBucket?.BucketIndex == bucketIndex)
            {
                // Already viewing this bucket - SetSelectedBucket is a no-op in this case, so
                // the "bucket changed" event that normally resolves the pending focus/edit
                // request above never fires. Reload directly instead.
                ReloadBucketAssets(bucketIndex);
                return;
            }

            _editorSubsystem.SetSelectedBucket(bucketIndex);
        }

        /// <summary>Opens the asset in a pop-out property editor panel, the same one used by the "Edit" button on asset-object properties in the inspector. Works for any AssetObject-derived type.</summary>
        private void OpenAssetEditor(AssetObjectViewModel? assetVm)
        {
            if (assetVm?.Bucket == null)
                return;

            uint bucketIndex = assetVm.Bucket.BucketIndex;
            Name assetName = assetVm.AssetDesc.Name;
            string displayName = assetVm.DisplayName;

            // The asset's actual TypeInfo can only be read on the sim thread, since it depends
            // on reading the live object rather than any statically-known managed type.
            _ = EngineManager.PostToSimThread(() =>
            {
                AssetRegistry registry = AssetManager.Instance.AssetRegistry;
                AssetObject? obj = registry.GetAsset(bucketIndex, assetName);

                if (obj == null || !obj.IsValid)
                {
                    return;
                }

                if (obj is ScriptAsset)
                {
                    string scriptPath = Path.Combine(registry.GetRootPath(), "Scripts", assetName.ToString() + ".hyp");

                    Dispatcher.UIThread.Post(() => CodeEditorService.OpenFile(scriptPath));
                    return;
                }

                TypeInfo typeInfo = obj.Class.TypeInfo;

                Dispatcher.UIThread.Post(() =>
                {
                    var propertyVm = new ObjectPropertyViewModel(
                        displayName,
                        typeInfo,
                        getter: () => new BoxedValue(AssetManager.Instance.AssetRegistry.GetAsset(bucketIndex, assetName)),
                        setter: _ => { },
                        isReadOnly: true);

                    propertyVm.RefreshValue();

                    var panel = new AssetObjectEditPanelViewModel(propertyVm);
                    PanelService.Instance.OpenPanel(panel);
                });
            });
        }
    }
}
