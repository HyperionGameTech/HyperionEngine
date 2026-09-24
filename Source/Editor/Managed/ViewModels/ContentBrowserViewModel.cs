using System;
using System.Collections.Generic;
using System.Collections.ObjectModel;
using System.Globalization;
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

        /// <summary>Raised whenever a newly-created asset is about to be focused, so the view can bring the content browser's dock tab to the front even if it's tabbed behind another panel.</summary>
        public static event Action? BringToFrontRequested;

        private readonly EditorSubsystem _editorSubsystem;
        private readonly ThumbnailService _thumbnailService;

        public ObservableCollection<AssetBucketViewModel> Buckets { get; } = new ObservableCollection<AssetBucketViewModel>();

        /// <summary>The assets shown in the browser: the current bucket's contents after the search filter and sort are applied.</summary>
        public ObservableCollection<AssetObjectViewModel> Assets { get; } = new ObservableCollection<AssetObjectViewModel>();

        /// <summary>Everything in the current bucket (or every bucket, when searching from the root), unfiltered - <see cref="Assets"/> is rebuilt from this.</summary>
        private readonly List<AssetObjectViewModel> _bucketAssets = new List<AssetObjectViewModel>();

        /// <summary>Set once <see cref="_bucketAssets"/> holds every bucket's assets, so a root search only loads them on its first keystroke.</summary>
        private bool _allBucketsLoaded;

        private AssetObjectViewModel? _selectedAsset;
        public AssetObjectViewModel? SelectedAsset
        {
            get => _selectedAsset;
            set => SetProperty(ref _selectedAsset, value);
        }

        private AssetBucketViewModel? _currentBucket;

        /// <summary>The bucket being browsed, or null when showing the root page of buckets.</summary>
        public AssetBucketViewModel? CurrentBucket
        {
            get => _currentBucket;
            set
            {
                _editorSubsystem.SetSelectedBucket(value?.BucketIndex ?? 0);
            }
        }

        private AssetBucketViewModel? _selectedBucket;

        /// <summary>The highlighted tile on the root page. Opening it is a separate step, like a folder.</summary>
        public AssetBucketViewModel? SelectedBucket
        {
            get => _selectedBucket;
            set => SetProperty(ref _selectedBucket, value);
        }

        public bool IsAtRoot => _currentBucket == null;

        public bool IsShowingBuckets => IsAtRoot && !HasSearchText;

        public bool IsShowingAssets => !IsShowingBuckets;

        public string SearchWatermark => _currentBucket != null
            ? $"Search {_currentBucket.Name}..."
            : "Search all assets...";

        private string _searchText = string.Empty;

        /// <summary>Substring the listed asset names are filtered by. Filters the current bucket, or every bucket from the root page.</summary>
        public string SearchText
        {
            get => _searchText;
            set
            {
                if (SetProperty(ref _searchText, value ?? string.Empty))
                {
                    OnPropertyChanged(nameof(HasSearchText));
                    OnPropertyChanged(nameof(IsShowingBuckets));
                    OnPropertyChanged(nameof(IsShowingAssets));

                    if (IsAtRoot)
                    {
                        if (!HasSearchText)
                        {
                            _thumbnailService.CancelPending();
                            _thumbnailService.ClearSubscribers();

                            _bucketAssets.Clear();
                            _allBucketsLoaded = false;
                        }
                        else if (!_allBucketsLoaded)
                        {
                            LoadAllBucketAssets();
                        }
                    }

                    ApplyFilterAndSort();
                }
            }
        }

        public bool HasSearchText => _searchText.Length != 0;

        /// <summary>True when a search is active and nothing matches it, so the empty list can be explained.</summary>
        public bool HasNoMatches => HasSearchText && Assets.Count == 0;

        public bool IsBucketEmpty => !IsAtRoot && !HasSearchText && Assets.Count == 0;

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
                    ApplyFilterAndSort();
                }
            }
        }


        private DelegateHandler? _onSelectedBucketChangedHandler;
        private DelegateHandler? _onAssetsChangedHandler;
        private DelegateHandler? _onProjectOpenedHandler;
        private DelegateHandler? _onProjectClosingHandler;
        private uint _pendingFocusBucket;
        private string? _pendingFocusNameHint;
        private bool _pendingOpenEditor;

        public ICommand ImportCommand { get; }

        public ICommand ClearSearchCommand { get; }

        public ICommand GoBackCommand { get; }
        public ICommand OpenBucketCommand { get; }

        public ICommand NewScriptCommand { get; }
        public ICommand NewMaterialCommand { get; }
        public ICommand NewWeaponCommand { get; }
        public ICommand NewDecalCommand { get; }
        public ICommand NewPhysicsShapeCommand { get; }
        public ICommand NewPrefabCommand { get; }

        public ICommand DeleteAssetCommand { get; }
        public ICommand EditAssetCommand { get; }

        public ICommand AddToSceneCommand { get; }

        /// <summary>Asset type name and create command for each bucket the browser can create into, keyed by bucket index.</summary>
        private readonly Dictionary<uint, (string TypeName, ICommand Command)> _newAssetActions;

        private (string TypeName, ICommand Command)? CurrentBucketNewAction =>
            _currentBucket != null && _newAssetActions.TryGetValue(_currentBucket.BucketIndex, out var newAction)
                ? newAction
                : null;

        /// <summary>Creates an asset of the open bucket's type. Null at the root, or for buckets whose assets are only made by importing.</summary>
        public ICommand? NewInBucketCommand => CurrentBucketNewAction?.Command;

        public string NewInBucketLabel => CurrentBucketNewAction is { } newAction ? $"New {newAction.TypeName}" : string.Empty;

        public bool HasNewInBucket => CurrentBucketNewAction != null;

        /// Simulation runs against a throwaway snapshot of the project, so anything authored while it
        /// runs would be thrown away with the snapshot.
        public bool CanCreateAssets => _editorSubsystem.CanCreateAssets();

        /// Re-checked on the sim thread because the panels that create assets can outlive the click
        /// that opened them, and simulation may have started in between.
        private bool CanCreateAssetsOnSimThread(string assetDescription)
        {
            if (_editorSubsystem.CanCreateAssets())
            {
                return true;
            }

            Logger.Log(LogLevel.Warning, $"Cannot create a new {assetDescription} while simulation is active.");

            return false;
        }

        public void RefreshCanCreateAssets()
        {
            Dispatcher.UIThread.VerifyAccess();

            OnPropertyChanged(nameof(CanCreateAssets));

            (NewScriptCommand as RelayCommand)?.RaiseCanExecuteChanged();
            (NewMaterialCommand as RelayCommand)?.RaiseCanExecuteChanged();
            (NewWeaponCommand as RelayCommand)?.RaiseCanExecuteChanged();
            (NewDecalCommand as RelayCommand)?.RaiseCanExecuteChanged();
            (NewPhysicsShapeCommand as RelayCommand)?.RaiseCanExecuteChanged();
            (NewPrefabCommand as RelayCommand)?.RaiseCanExecuteChanged();
        }

        public ContentBrowserViewModel(EditorSubsystem editorSubsystem)
        {
            _editorSubsystem = editorSubsystem ?? throw new ArgumentNullException(nameof(editorSubsystem));

            ImportCommand = new EditorCommand("ImportContent");

            ClearSearchCommand = new RelayCommand(() => SearchText = string.Empty);

            GoBackCommand = new RelayCommand(GoBack, () => !IsAtRoot);

            OpenBucketCommand = new RelayCommand<AssetBucketViewModel>(bucketVm =>
            {
                if (bucketVm != null)
                {
                    CurrentBucket = bucketVm;
                }
            });

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
                var panel = new NewScriptPanelViewModel((name, languageArg) =>
                {
                    if (string.IsNullOrEmpty(name) || string.IsNullOrEmpty(languageArg))
                    {
                        Logger.Log(LogLevel.Warning, "New script creation cancelled.");
                        return;
                    }

                    _ = EngineManager.PostToSimThread(() =>
                    {
                        if (!CanCreateAssetsOnSimThread("script"))
                        {
                            return;
                        }

                        _editorSubsystem.ExecuteCommandByName(new Name("EditorCommandNewScript"), $"{languageArg} {name}");

                        Dispatcher.UIThread.Post(() => FocusAsset(AssetBucket.Scripts.Value, name));
                    });
                });

                PanelService.Instance.OpenPanel(panel);
            }, () => CanCreateAssets);

            NewWeaponCommand = new RelayCommand(() =>
            {
                var panel = new NewWeaponPanelViewModel(weaponType =>
                {
                    if (weaponType == null)
                    {
                        Logger.Log(LogLevel.Warning, "New weapon creation cancelled.");
                        return;
                    }

                    _ = EngineManager.PostToSimThread(() =>
                    {
                        if (!CanCreateAssetsOnSimThread("weapon"))
                        {
                            return;
                        }

                        // EditorCommandNewWeapon takes the WeaponType as an integer
                        _editorSubsystem.ExecuteCommandByName(new Name("EditorCommandNewWeapon"), Convert.ToString((int)weaponType.Value, CultureInfo.InvariantCulture));

                        Dispatcher.UIThread.Post(() => FocusAsset(AssetBucket.Weapons.Value, "NewWeapon", openEditor: true));
                    });
                });

                PanelService.Instance.OpenPanel(panel);
            }, () => CanCreateAssets);

            NewDecalCommand = new RelayCommand(() =>
            {
                _ = EngineManager.PostToSimThread(() =>
                {
                    if (!CanCreateAssetsOnSimThread("decal"))
                    {
                        return;
                    }

                    AssetRegistry registry = AssetManager.Instance.AssetRegistry;
                    uint bucketIndex = AssetBucket.Decals.Value;

                    // the command picks a unique name, diff the bucket to find it
                    var namesBefore = new HashSet<string>(
                        registry.GetBucketAssetDescs(bucketIndex).Select(assetDesc => assetDesc.Name.ToString()),
                        StringComparer.Ordinal);

                    _editorSubsystem.ExecuteCommandByName(new Name("EditorCommandNewDecal"));

                    string? createdName = registry.GetBucketAssetDescs(bucketIndex)
                        .Select(assetDesc => assetDesc.Name.ToString())
                        .FirstOrDefault(name => !namesBefore.Contains(name));

                    if (createdName == null)
                    {
                        Logger.Log(LogLevel.Error, "New decal creation failed; no new asset appeared in the decals bucket.");
                        return;
                    }

                    Dispatcher.UIThread.Post(() => FocusAsset(bucketIndex, createdName, openEditor: true));
                });
            }, () => CanCreateAssets);

            NewMaterialCommand = new RelayCommand(() =>
            {
                _ = EngineManager.PostToSimThread(() =>
                {
                    if (!CanCreateAssetsOnSimThread("material"))
                    {
                        return;
                    }

                    AssetRegistry registry = AssetManager.Instance.AssetRegistry;
                    uint bucketIndex = AssetBucket.Materials.Value;

                    // EditorCommandNewMaterial picks a unique name for the material itself and gives us
                    // no way to ask for it, so diff the bucket to find out what it ended up being.
                    var namesBefore = new HashSet<string>(
                        registry.GetBucketAssetDescs(bucketIndex).Select(assetDesc => assetDesc.Name.ToString()),
                        StringComparer.Ordinal);

                    _editorSubsystem.ExecuteCommandByName(new Name("EditorCommandNewMaterial"));

                    string? createdName = registry.GetBucketAssetDescs(bucketIndex)
                        .Select(assetDesc => assetDesc.Name.ToString())
                        .FirstOrDefault(name => !namesBefore.Contains(name));

                    if (createdName == null)
                    {
                        Logger.Log(LogLevel.Error, "New material creation failed; no new asset appeared in the materials bucket.");
                        return;
                    }

                    Dispatcher.UIThread.Post(() => FocusAsset(bucketIndex, createdName, openEditor: true));
                });
            }, () => CanCreateAssets);

            NewPhysicsShapeCommand = new RelayCommand(() =>
            {
                var panel = new NewPhysicsShapePanelViewModel(shape =>
                {
                    if (shape == null)
                    {
                        Logger.Log(LogLevel.Warning, "Physics shape creation cancelled.");
                        return;
                    }

                    _ = EngineManager.PostToSimThread(() =>
                    {
                        if (!CanCreateAssetsOnSimThread("physics shape"))
                        {
                            return;
                        }

                        AssetRegistry? registry = EngineManager.EditorGame?.AssetRegistry;
                        Debug.Assert(registry != null);

                        registry.PutAssetUnique(shape);

                        Dispatcher.UIThread.Post(() => FocusAsset(AssetBucket.PhysicsShapes.Value, shape.GetName().ToString()));
                    });
                });

                PanelService.Instance.OpenPanel(panel);
            }, () => CanCreateAssets);

            NewPrefabCommand = new RelayCommand(() =>
            {
                _ = EngineManager.PostToSimThread(() =>
                {
                    if (!CanCreateAssetsOnSimThread("prefab"))
                    {
                        return;
                    }

                    AssetRegistry registry = AssetManager.Instance.AssetRegistry;
                    uint bucketIndex = AssetBucket.Prefabs.Value;

                    /// @TODO: Use same technique for naming the other types.
                    var existingNames = new HashSet<string>(
                        registry.GetBucketAssetDescs(bucketIndex).Select(assetDesc => assetDesc.Name.ToString()),
                        StringComparer.Ordinal);

                    string name = "NewPrefab";
                    for (int suffix = 1; existingNames.Contains(name); suffix++)
                    {
                        name = $"NewPrefab_{suffix}";
                    }

                    _editorSubsystem.ExecuteCommandByName(new Name("EditorCommandNewPrefab"), name);

                    Dispatcher.UIThread.Post(() => FocusAsset(bucketIndex, name));
                });
            }, () => CanCreateAssets);

            _newAssetActions = new Dictionary<uint, (string TypeName, ICommand Command)>
            {
                [AssetBucket.PhysicsShapes.Value] = ("Physics Shape", NewPhysicsShapeCommand),
                [AssetBucket.Materials.Value] = ("Material", NewMaterialCommand),
                [AssetBucket.Weapons.Value] = ("Weapon", NewWeaponCommand),
                [AssetBucket.Decals.Value] = ("Decal", NewDecalCommand),
                [AssetBucket.Scripts.Value] = ("Script", NewScriptCommand),
                [AssetBucket.Prefabs.Value] = ("Prefab", NewPrefabCommand),
            };

            AddToSceneCommand = new RelayCommand<AssetObjectViewModel>(asset =>
            {
                if (asset?.Bucket == null)
                {
                    return;
                }

                // add asset to scene by invoking the EditorCommand
                _editorSubsystem.ExecuteCommandByName(new Name("EditorCommandAddAsset"), $"{asset.Bucket.BucketIndex} {asset.AssetDesc.Name}");
            });

            _thumbnailService = new ThumbnailService(editorSubsystem);

            Instance = this;
        }

        public void LoadBuckets()
        {
            Dispatcher.UIThread.VerifyAccess();

            Logger.Log(LogLevel.Verbose, "Loading content browser buckets...");

            Buckets.Clear();
            _bucketAssets.Clear();
            Assets.Clear();

            foreach (AssetBucket bucket in AssetBucket.AllBuckets)
            {
                Buckets.Add(new AssetBucketViewModel(bucket));
            }

            OnPropertyChanged(nameof(Buckets));

            RefreshBucketCounts();

            _onSelectedBucketChangedHandler = _editorSubsystem.GetOnSelectedBucketChangedDelegate().Bind((uint bucketIndex) =>
            {
                Logger.Log(LogLevel.Verbose, "Selected bucket changed: {0}", AssetBucket.GetAssetBucketName(bucketIndex));

                Dispatcher.UIThread.Post(() => ReloadBucketAssets(bucketIndex));
            });

            _onAssetsChangedHandler = _editorSubsystem.GetOnAssetsChangedDelegate().Bind((uint bucketIndex) =>
            {
                Dispatcher.UIThread.Post(() =>
                {
                    RefreshBucketCount(bucketIndex);

                    // a root search lists every bucket, so any change may touch it
                    if (_currentBucket?.BucketIndex == bucketIndex || (IsAtRoot && HasSearchText))
                    {
                        // An asset in this bucket was added, removed or edited - the decoded images we
                        // are holding may no longer match what is on disk.
                        _thumbnailService.Clear();

                        RefreshAssets();
                    }
                });
            });

            _onProjectOpenedHandler = _editorSubsystem.GetOnProjectOpenedDelegate().Bind((EditorProject project) =>
            {
                Dispatcher.UIThread.Post(() =>
                {
                    RefreshBucketCounts();
                    RefreshAssets();
                });
            });

            _onProjectClosingHandler = _editorSubsystem.GetOnProjectClosingDelegate().Bind((EditorProject project) =>
            {
                Dispatcher.UIThread.Post(() =>
                {
                    _thumbnailService.Clear();

                    _bucketAssets.Clear();
                    _allBucketsLoaded = false;
                    Assets.Clear();
                    SelectedAsset = null;

                    foreach (AssetBucketViewModel bucketVm in Buckets)
                    {
                        bucketVm.AssetCount = 0;
                    }

                    OnPropertyChanged(nameof(Assets));
                    OnPropertyChanged(nameof(HasNoMatches));
                    OnPropertyChanged(nameof(IsBucketEmpty));
                });
            });
        }

        private void RefreshBucketCounts()
        {
            foreach (AssetBucketViewModel bucketVm in Buckets)
            {
                RefreshBucketCount(bucketVm.BucketIndex);
            }
        }

        private void RefreshBucketCount(uint bucketIndex)
        {
            AssetBucketViewModel? bucketVm = Buckets.FirstOrDefault(bvm => bvm.BucketIndex == bucketIndex);

            if (bucketVm != null)
            {
                bucketVm.AssetCount = (int)AssetManager.Instance.AssetRegistry.GetBucketAssetCount(bucketIndex);
            }
        }

        /// <summary>Returns to the root page of buckets, keeping the search so it widens to every bucket.</summary>
        private void GoBack()
        {
            if (IsAtRoot)
            {
                return;
            }

            // lets the bucket just left stay highlighted on the root page
            SelectedBucket = _currentBucket;

            CurrentBucket = null;
        }

        /// <summary>Reloads the asset list for the given bucket and resolves any pending focus/edit request. Must run on the UI thread.</summary>
        private void ReloadBucketAssets(uint bucketIndex)
        {
            // Whatever was still queued, and whatever is subscribed, belongs to the asset set we are
            // about to replace.
            _thumbnailService.CancelPending();
            _thumbnailService.ClearSubscribers();

            _bucketAssets.Clear();
            _allBucketsLoaded = false;
            Assets.Clear();
            SelectedAsset = null;

            _currentBucket = bucketIndex != 0
                ? Buckets.FirstOrDefault(bvm => bvm.BucketIndex == bucketIndex)
                : null;

            if (_currentBucket != null)
            {
                LoadBucketAssets(_currentBucket);
            }
            else if (HasSearchText)
            {
                LoadAllBucketAssets();
            }

            ApplyFilterAndSort();

            // Handle pending focus after asset creation
            if (bucketIndex != 0 && _pendingFocusBucket == bucketIndex)
            {
                _pendingFocusBucket = 0;

                if (_pendingFocusNameHint != null)
                {
                    // Prefer an exact match: unique asset names are generated by appending a suffix,
                    // so a prefix match on "NewMaterial" would also hit "NewMaterial_1".
                    SelectedAsset = Assets.FirstOrDefault(a =>
                        string.Equals(a.DisplayName, _pendingFocusNameHint, StringComparison.OrdinalIgnoreCase))
                        ?? Assets.FirstOrDefault(a =>
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

            OnPropertyChanged(nameof(CurrentBucket));
            OnPropertyChanged(nameof(IsAtRoot));
            OnPropertyChanged(nameof(IsShowingBuckets));
            OnPropertyChanged(nameof(IsShowingAssets));
            OnPropertyChanged(nameof(IsBucketEmpty));
            OnPropertyChanged(nameof(SearchWatermark));
            OnPropertyChanged(nameof(NewInBucketCommand));
            OnPropertyChanged(nameof(NewInBucketLabel));
            OnPropertyChanged(nameof(HasNewInBucket));

            (GoBackCommand as RelayCommand)?.RaiseCanExecuteChanged();
        }

        /// <summary>Appends the given bucket's assets to <see cref="_bucketAssets"/>. Must run on the UI thread.</summary>
        private void LoadBucketAssets(AssetBucketViewModel bucketVm)
        {
            AssetRegistry registry = AssetManager.Instance.AssetRegistry;
            string rootPath = registry.GetRootPath();

            List<AssetObjectViewModel> unsavedAssets = [];
            int assetCount = 0;

            foreach (AssetDesc assetDesc in registry.GetBucketAssetDescs(bucketVm.BucketIndex))
            {
                string manifestPath = Path.Combine(rootPath, bucketVm.Name, assetDesc.Name.ToString() + ".hmf");

                DateTime? dateModified = null;
                string? typeName = null;

                bool hasManifest = File.Exists(manifestPath);

                if (hasManifest)
                {
                    dateModified = File.GetLastWriteTime(manifestPath);
                    typeName = ReadAssetTypeName(manifestPath);
                }

                AssetObjectViewModel assetVm = new AssetObjectViewModel(assetDesc, bucketVm, typeName, dateModified);
                _bucketAssets.Add(assetVm);
                assetCount++;

                if (!hasManifest)
                {
                    unsavedAssets.Add(assetVm);
                }
            }

            bucketVm.AssetCount = assetCount;

            if (unsavedAssets.Count > 0)
            {
                ResolveUnsavedAssetTypeNames(bucketVm.BucketIndex, unsavedAssets);
            }
        }

        private void LoadAllBucketAssets()
        {
            foreach (AssetBucketViewModel bucketVm in Buckets)
            {
                LoadBucketAssets(bucketVm);
            }

            _allBucketsLoaded = true;
        }

        /// <summary>
        /// Assets created this session but not yet saved have no manifest to read a type from, so the type
        /// comes from the live object in the registry instead.
        /// </summary>
        private static void ResolveUnsavedAssetTypeNames(uint bucketIndex, List<AssetObjectViewModel> assetVms)
        {
            _ = EngineManager.PostToSimThread(() =>
            {
                AssetRegistry registry = AssetManager.Instance.AssetRegistry;

                List<(AssetObjectViewModel AssetVm, string TypeName)> resolved = new List<(AssetObjectViewModel, string)>();

                foreach (AssetObjectViewModel assetVm in assetVms)
                {
                    AssetObject? obj = registry.GetAsset(bucketIndex, assetVm.AssetDesc.Name);

                    if (obj != null && obj.IsValid)
                    {
                        resolved.Add((assetVm, obj.Class.Name.ToString()));
                    }
                }

                if (resolved.Count == 0)
                {
                    return;
                }

                Dispatcher.UIThread.Post(() =>
                {
                    foreach ((AssetObjectViewModel assetVm, string typeName) in resolved)
                    {
                        assetVm.SetTypeName(typeName);
                    }
                });
            });
        }

        /// <summary>
        /// Reads an asset's concrete class name out of its .hmf manifest without deserializing it.
        /// HMF is a text format whose first token is the class name, e.g. <c>Material "CubeMaterial" {</c>,
        /// so a short read off the front is enough. Returns null if the file can't be read.
        /// </summary>
        private static string? ReadAssetTypeName(string manifestPath)
        {
            Span<char> buffer = stackalloc char[128];
            int count;

            try
            {
                using var reader = new StreamReader(manifestPath);

                count = reader.Read(buffer);
            }
            catch (Exception)
            {
                // An unreadable manifest just means no type icon; the asset still lists.
                return null;
            }

            for (int i = 0; i < count; i++)
            {
                char c = buffer[i];

                // The class name runs until the first separator - whitespace, the quoted asset name,
                // or the opening brace when the asset is anonymous.
                if (char.IsWhiteSpace(c) || c == '"' || c == '{')
                {
                    return i > 0 ? new string(buffer.Slice(0, i)) : null;
                }
            }

            return null;
        }

        /// <summary>Requests preview images for the currently listed assets. Must run on the UI thread.</summary>
        private void RequestThumbnails()
        {
            foreach (AssetObjectViewModel assetVm in Assets)
            {
                assetVm.RequestThumbnail();
            }
        }

        /// <summary>Reloads the listed assets (the current bucket, or every bucket for a root search), preserving the selection where possible. Must run on the UI thread.</summary>
        private void RefreshAssets()
        {
            Dispatcher.UIThread.VerifyAccess();

            if (IsShowingBuckets)
            {
                return;
            }

            Name selectedName = SelectedAsset?.AssetDesc.Name ?? Name.Invalid;
            uint selectedBucketIndex = SelectedAsset?.Bucket?.BucketIndex ?? 0;

            ReloadBucketAssets(_currentBucket?.BucketIndex ?? 0);

            if (selectedName.Valid)
            {
                SelectedAsset = Assets.FirstOrDefault(a => a.AssetDesc.Name == selectedName && a.Bucket?.BucketIndex == selectedBucketIndex);
            }
        }

        /// <summary>Rebuilds <see cref="Assets"/> from the current bucket's contents, honouring the search text and sort mode. Must run on the UI thread.</summary>
        private void ApplyFilterAndSort()
        {
            AssetObjectViewModel? preserved = SelectedAsset;

            IEnumerable<AssetObjectViewModel> matching = IsShowingBuckets
                ? Enumerable.Empty<AssetObjectViewModel>()
                : _bucketAssets;

            if (HasSearchText)
            {
                matching = matching.Where(a => a.DisplayName.Contains(_searchText, StringComparison.OrdinalIgnoreCase));
            }

            List<AssetObjectViewModel> sorted = _sortMode switch
            {
                AssetSortMode.DateModified =>
                    matching.OrderByDescending(a => a.DateModified ?? DateTime.MinValue).ToList(),
                AssetSortMode.Type =>
                    matching.OrderBy(a => a.TypeName ?? string.Empty, StringComparer.OrdinalIgnoreCase)
                            .ThenBy(a => a.DisplayName, StringComparer.OrdinalIgnoreCase)
                            .ToList(),
                _ /* Name */ =>
                    matching.OrderBy(a => a.DisplayName, StringComparer.OrdinalIgnoreCase).ToList(),
            };

            Assets.Clear();
            foreach (AssetObjectViewModel assetVm in sorted)
                Assets.Add(assetVm);

            // The filter may have just hidden whatever was selected.
            SelectedAsset = preserved != null && Assets.Contains(preserved) ? preserved : null;

            OnPropertyChanged(nameof(Assets));
            OnPropertyChanged(nameof(HasNoMatches));
            OnPropertyChanged(nameof(IsBucketEmpty));

            RequestThumbnails();
        }


        public void Dispose()
        {
            _thumbnailService.Dispose();

            _onSelectedBucketChangedHandler?.Remove();
            _onSelectedBucketChangedHandler?.Dispose();
            _onAssetsChangedHandler?.Remove();
            _onAssetsChangedHandler?.Dispose();
            _onProjectOpenedHandler?.Remove();
            _onProjectOpenedHandler?.Dispose();
            _onProjectClosingHandler?.Remove();
            _onProjectClosingHandler?.Dispose();
        }

        /// <summary>Switches to the given bucket and focuses the named asset once loaded.</summary>
        public void FocusAsset(uint bucketIndex, string? nameHint = null, bool openEditor = false)
        {
            Dispatcher.UIThread.VerifyAccess();

            if (bucketIndex == 0)
                return;

            BringToFrontRequested?.Invoke();

            // An active search would hide the asset we are about to select.
            SearchText = string.Empty;

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
                    Logger.Log(LogLevel.Warning, $"EditAsset: asset '{displayName}' could not be resolved.");
                    return;
                }

                if (obj is ScriptAsset scriptAsset)
                {
                    ScriptDesc scriptDesc = scriptAsset.ScriptDesc;

                    string scriptPath = Path.Combine(registry.GetRootPath(), scriptDesc.Path);
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
