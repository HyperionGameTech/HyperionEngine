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
    public sealed record AssetCreationEntry(string Header, ICommand Command, bool IsUserDefined = false);

    internal sealed record CreatableAssetType(string Name, bool IsUserDefined, IReadOnlyList<ClassPickerEntry>? PickerEntries);

    public enum AssetSortMode
    {
        Name,
        DateModified,
        Type,
    }

    public class AssetTypeFilterViewModel : ViewModelBase
    {
        private readonly Action _onChanged;

        public uint BucketIndex { get; }

        public string Name { get; }

        private bool _isChecked;
        public bool IsChecked
        {
            get => _isChecked;
            set
            {
                if (SetProperty(ref _isChecked, value))
                {
                    _onChanged();
                }
            }
        }

        public AssetTypeFilterViewModel(AssetBucket bucket, Action onChanged)
        {
            BucketIndex = bucket.Index;
            Name = bucket.Name;
            _onChanged = onChanged;
        }
    }

    public class ContentBrowserViewModel : ViewModelBase, IDisposable
    {
        public static ContentBrowserViewModel? Instance { get; private set; }

        /// <summary>Raised whenever a newly-created asset is about to be focused, so the view can bring the content browser's dock tab to the front even if it's tabbed behind another panel.</summary>
        public static event Action? BringToFrontRequested;

        private readonly EditorSubsystem _editorSubsystem;
        private readonly ThumbnailService _thumbnailService;

        private readonly Dictionary<uint, AssetBucketViewModel> _buckets = new Dictionary<uint, AssetBucketViewModel>();

        private readonly Dictionary<string, AssetFolderViewModel> _folderIndex = new Dictionary<string, AssetFolderViewModel>(StringComparer.Ordinal);

        private readonly List<AssetObjectViewModel> _allAssets = new List<AssetObjectViewModel>();

        private readonly Stack<string> _backHistory = new Stack<string>();
        private readonly Stack<string> _forwardHistory = new Stack<string>();

        private static readonly KeyValuePair<string, Func<ContentBrowserViewModel, ICommand>>[] DedicatedCreatorTypes =
        {
            new(nameof(ScriptAsset), (ContentBrowserViewModel vm) => vm.NewScriptCommand),
            new(nameof(Weapon), (ContentBrowserViewModel vm) => vm.NewWeaponCommand),
            new(nameof(World), (ContentBrowserViewModel vm) => vm.NewWorldCommand)
        };

        public AssetFolderViewModel RootFolder { get; private set; } = new AssetFolderViewModel(string.Empty, null);

        /// <summary>The tree's top level: just <see cref="RootFolder"/>.</summary>
        public ObservableCollection<AssetFolderViewModel> RootFolders { get; } = new ObservableCollection<AssetFolderViewModel>();

        /// <summary>What the grid shows: the current folder's subfolders, then its assets (or the search results).</summary>
        public ObservableCollection<object> Items { get; } = new ObservableCollection<object>();

        /// <summary>Root to current folder, for the path bar.</summary>
        public ObservableCollection<AssetFolderViewModel> PathSegments { get; } = new ObservableCollection<AssetFolderViewModel>();

        public ObservableCollection<AssetTypeFilterViewModel> TypeFilters { get; } = new ObservableCollection<AssetTypeFilterViewModel>();

        private AssetFolderViewModel _currentFolder;
        public AssetFolderViewModel CurrentFolder => _currentFolder;

        /// <summary>The tree shows the open folder as its selection.</summary>
        public AssetFolderViewModel SelectedTreeFolder => _currentFolder;

        private object? _selectedItem;
        public object? SelectedItem
        {
            get => _selectedItem;
            set
            {
                if (SetProperty(ref _selectedItem, value))
                {
                    OnPropertyChanged(nameof(SelectedAsset));
                }
            }
        }

        public AssetObjectViewModel? SelectedAsset
        {
            get => _selectedItem as AssetObjectViewModel;
            set => SelectedItem = value;
        }

        public string SearchWatermark => _currentFolder.IsRoot ? "Search all assets..." : $"Search {_currentFolder.Name}...";

        private string _searchText = string.Empty;

        /// <summary>Substring the listed names are filtered by. Searches the current folder and everything below it.</summary>
        public string SearchText
        {
            get => _searchText;
            set
            {
                if (SetProperty(ref _searchText, value ?? string.Empty))
                {
                    OnPropertyChanged(nameof(HasSearchText));
                    RebuildItems();
                }
            }
        }

        public bool HasSearchText => _searchText.Length != 0;

        public bool HasTypeFilter => TypeFilters.Any(filter => filter.IsChecked);

        public bool HasNoMatches => (HasSearchText || HasTypeFilter) && Items.Count == 0;

        public bool IsFolderEmpty => !HasSearchText && !HasTypeFilter && Items.Count == 0;

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
                    RebuildItems();
                }
            }
        }

        private EditorAssetFolders? _assetFolders;

        private DelegateHandler? _onAssetsChangedHandler;
        private DelegateHandler? _onFoldersChangedHandler;
        private DelegateHandler? _onProjectOpenedHandler;
        private DelegateHandler? _onProjectClosingHandler;

        private bool _isReloadQueued;

        private uint _pendingFocusBucket;
        private string? _pendingFocusName;
        private bool _pendingOpenEditor;
        private bool _pendingBeginRename;

        public ICommand ImportCommand { get; }

        public ICommand ClearSearchCommand { get; }
        public ICommand ClearTypeFilterCommand { get; }

        public ICommand GoBackCommand { get; }
        public ICommand GoForwardCommand { get; }
        public ICommand GoUpCommand { get; }
        public ICommand OpenFolderCommand { get; }

        public ICommand NewScriptCommand { get; }
        public ICommand NewWeaponCommand { get; }
        public ICommand NewDecalCommand { get; }
        public ICommand NewWorldCommand { get; }

        public ICommand OpenWorldCommand { get; }

        public ICommand DeleteAssetCommand { get; }
        public ICommand EditAssetCommand { get; }

        public ICommand NewFolderCommand { get; }
        public ICommand DeleteFolderCommand { get; }
        public ICommand DeleteSelectionCommand { get; }
        public ICommand ShowInExplorerCommand { get; }

        /// <summary>Raised when a folder's name should become editable, so the view can focus its text box.</summary>
        public static event Action<AssetFolderViewModel>? FolderRenameRequested;

        /// <summary>Raised when a newly created asset's name should become editable, so the view can focus its text box.</summary>
        public static event Action<AssetObjectViewModel>? AssetRenameRequested;

        private string? _pendingRenamePath;

        /// <summary>The grid's current selection, kept by the view; <see cref="SelectedItem"/> is only its anchor.</summary>
        public IReadOnlyList<object> SelectedItems { get; set; } = Array.Empty<object>();

        public ICommand AddToSceneCommand { get; }

        public ICommand PlaceAsInstanceCommand { get; }

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
            (NewWeaponCommand as RelayCommand)?.RaiseCanExecuteChanged();
            (NewDecalCommand as RelayCommand)?.RaiseCanExecuteChanged();
            (NewWorldCommand as RelayCommand)?.RaiseCanExecuteChanged();
            (NewFolderCommand as RelayCommand<AssetFolderViewModel>)?.RaiseCanExecuteChanged();
        }

        public ContentBrowserViewModel(EditorSubsystem editorSubsystem)
        {
            _editorSubsystem = editorSubsystem ?? throw new ArgumentNullException(nameof(editorSubsystem));

            _currentFolder = RootFolder;
            RootFolders.Add(RootFolder);

            foreach (AssetBucket bucket in AssetBucket.AllBuckets)
            {
                _buckets[bucket.Index] = new AssetBucketViewModel(bucket);
                TypeFilters.Add(new AssetTypeFilterViewModel(bucket, OnTypeFilterChanged));
            }

            ImportCommand = new EditorCommand("ImportContent");

            ClearSearchCommand = new RelayCommand(() => SearchText = string.Empty);

            ClearTypeFilterCommand = new RelayCommand(() =>
            {
                foreach (AssetTypeFilterViewModel filter in TypeFilters)
                {
                    filter.IsChecked = false;
                }
            });

            GoBackCommand = new RelayCommand(GoBack, () => _backHistory.Count > 0);
            GoForwardCommand = new RelayCommand(GoForward, () => _forwardHistory.Count > 0);
            GoUpCommand = new RelayCommand(GoUp, () => !_currentFolder.IsRoot);

            OpenFolderCommand = new RelayCommand<AssetFolderViewModel>(folder =>
            {
                if (folder != null)
                {
                    NavigateTo(folder);
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

            NewFolderCommand = new RelayCommand<AssetFolderViewModel>(parent =>
            {
                CreateFolder(parent ?? _currentFolder);
            }, _ => CanCreateAssets);

            DeleteFolderCommand = new RelayCommand<AssetFolderViewModel>(folder =>
            {
                if (folder != null && !folder.IsRoot)
                {
                    DeleteItems(new object[] { folder });
                }
            });

            DeleteSelectionCommand = new RelayCommand<object>(clicked =>
            {
                List<object> items = SelectedItems.Count > 0 && (clicked == null || SelectedItems.Contains(clicked))
                    ? SelectedItems.ToList()
                    : (clicked != null ? new List<object> { clicked } : new List<object>());

                DeleteItems(items);
            });

            ShowInExplorerCommand = new RelayCommand<AssetObjectViewModel>(asset =>
            {
                if (asset?.Bucket == null)
                {
                    return;
                }

                string manifestPath = Path.Combine(AssetManager.Instance.AssetRegistry.GetRootPath(), asset.Bucket.Name, asset.DisplayName + ".hmf");

                ShellService.RevealInFileManager(File.Exists(manifestPath) ? manifestPath : Path.GetDirectoryName(manifestPath)!);
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

                    CreateInCurrentFolder("script", AssetBucket.Scripts.Value, openEditor: false, () =>
                    {
                        _editorSubsystem.ExecuteCommandByName(new Name("EditorCommandNewScript"), $"{languageArg} {name}");
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

                    CreateInCurrentFolder("weapon", AssetBucket.Weapons.Value, openEditor: true, () =>
                    {
                        // EditorCommandNewWeapon takes the WeaponType as an integer
                        _editorSubsystem.ExecuteCommandByName(new Name("EditorCommandNewWeapon"), Convert.ToString((int)weaponType.Value, CultureInfo.InvariantCulture));
                    });
                });

                PanelService.Instance.OpenPanel(panel);
            }, () => CanCreateAssets);

            NewDecalCommand = new RelayCommand(() => CreateAssetOfClass("Decal"), () => CanCreateAssets);

            NewWorldCommand = new RelayCommand(() =>
            {
                CreateInCurrentFolder("world", AssetBucket.Worlds.Value, openEditor: false, () =>
                {
                    _editorSubsystem.ExecuteCommandByName(new Name("EditorCommandNewWorld"), MakeUniqueAssetName(AssetBucket.Worlds.Value, "NewWorld"));
                });
            }, () => CanCreateAssets);

            OpenWorldCommand = new RelayCommand<AssetObjectViewModel>(asset =>
            {
                if (asset == null || !asset.IsWorld)
                {
                    return;
                }

                _editorSubsystem.ExecuteCommandByName(new Name("EditorCommandOpenWorld"), asset.AssetDesc.Name.ToString());
            });

            AddToSceneCommand = new RelayCommand<AssetObjectViewModel>(asset =>
            {
                if (asset?.Bucket == null || !asset.IsPrefab)
                {
                    return;
                }

                // add asset to scene by invoking the EditorCommand
                _editorSubsystem.ExecuteCommandByName(new Name("EditorCommandAddAsset"), $"{asset.Bucket.BucketIndex} {asset.AssetDesc.Name}");
            });

            PlaceAsInstanceCommand = new RelayCommand<AssetObjectViewModel>(asset =>
            {
                if (asset?.Bucket == null || !asset.IsPrefab)
                {
                    return;
                }

                _editorSubsystem.ExecuteCommandByName(new Name("EditorCommandPlaceAsInstance"), $"{asset.Bucket.BucketIndex} {asset.AssetDesc.Name}");
            });

            _thumbnailService = new ThumbnailService(editorSubsystem);

            Instance = this;
        }

        public void Initialize()
        {
            Dispatcher.UIThread.VerifyAccess();

            _onAssetsChangedHandler = _editorSubsystem.GetOnAssetsChangedDelegate().Bind((uint bucketIndex) => NotifyAssetsChanged());

            _onProjectOpenedHandler = _editorSubsystem.GetOnProjectOpenedDelegate().Bind((EditorProject project) =>
            {
                Dispatcher.UIThread.Post(() => BindProject(project));
            });

            _onProjectClosingHandler = _editorSubsystem.GetOnProjectClosingDelegate().Bind((EditorProject project) =>
            {
                Dispatcher.UIThread.Post(() => BindProject(null));
            });

            BindProject(EngineManager.CurrentProject);
        }

        private sealed class BrowserState
        {
            public string CurrentFolder { get; set; } = string.Empty;

            public List<string> ExpandedFolders { get; set; } = new List<string>();
        }

        private string? _boundProjectFilePath;

        private static string? GetBrowserStatePath(string? projectFilePath)
        {
            return string.IsNullOrEmpty(projectFilePath)
                ? null
                : Path.Combine(Path.GetDirectoryName(projectFilePath)!, "Saved", "ContentBrowser.json");
        }

        /// <summary>Remembers the open folder and the expanded ones in the project's Saved folder, for the next time it is opened.</summary>
        private void SaveBrowserState()
        {
            if (GetBrowserStatePath(_boundProjectFilePath) is not { } statePath || _assetFolders == null)
            {
                return;
            }

            BrowserState state = new BrowserState
            {
                CurrentFolder = _currentFolder.Path,
                ExpandedFolders = _folderIndex.Values.Where(folder => folder.IsExpanded).Select(folder => folder.Path).OrderBy(path => path, StringComparer.Ordinal).ToList()
            };

            try
            {
                Directory.CreateDirectory(Path.GetDirectoryName(statePath)!);
                File.WriteAllText(statePath, System.Text.Json.JsonSerializer.Serialize(state));
            }
            catch (Exception ex)
            {
                Logger.Log(LogLevel.Warning, $"Could not save content browser state to {statePath}: {ex.Message}");
            }
        }

        private static BrowserState? LoadBrowserState(string? projectFilePath)
        {
            if (GetBrowserStatePath(projectFilePath) is not { } statePath || !File.Exists(statePath))
            {
                return null;
            }

            try
            {
                return System.Text.Json.JsonSerializer.Deserialize<BrowserState>(File.ReadAllText(statePath));
            }
            catch (Exception)
            {
                return null;
            }
        }

        private void BindProject(EditorProject? project)
        {
            Dispatcher.UIThread.VerifyAccess();

            SaveBrowserState();

            _onFoldersChangedHandler?.Remove();
            _onFoldersChangedHandler?.Dispose();
            _onFoldersChangedHandler = null;

            _thumbnailService.Clear();

            _assetFolders = project?.AssetFolders;
            _boundProjectFilePath = project != null && project.IsSaved ? project.FilePath : null;

            _backHistory.Clear();
            _forwardHistory.Clear();

            if (_assetFolders != null)
            {
                _onFoldersChangedHandler = _assetFolders.GetOnFoldersChangedDelegate().Bind(() =>
                {
                    Dispatcher.UIThread.Post(QueueReload);
                });
            }

            BrowserState? state = LoadBrowserState(_boundProjectFilePath);

            _folderIndex.Clear();

            ReloadAll(state?.CurrentFolder ?? string.Empty, state?.ExpandedFolders);
        }

        /// <summary>Reloads once, however many changes arrive before the UI thread gets to it.</summary>
        private void QueueReload()
        {
            Dispatcher.UIThread.VerifyAccess();

            if (_isReloadQueued)
            {
                return;
            }

            _isReloadQueued = true;

            Dispatcher.UIThread.Post(() =>
            {
                _isReloadQueued = false;

                ReloadAll(_currentFolder.Path);
            }, DispatcherPriority.Background);
        }

        private void ReloadAll(string folderPath, IEnumerable<string>? restoredExpandedPaths = null)
        {
            Dispatcher.UIThread.VerifyAccess();

            HashSet<string> expandedPaths = restoredExpandedPaths != null
                ? new HashSet<string>(restoredExpandedPaths, StringComparer.Ordinal)
                : new HashSet<string>(_folderIndex.Values.Where(folder => folder.IsExpanded).Select(folder => folder.Path), StringComparer.Ordinal);

            if (_folderIndex.Count == 0)
            {
                expandedPaths.Add(string.Empty);
            }

            object? previousSelection = _selectedItem;

            RebuildFolderTree(expandedPaths);
            RebuildAssetIndex();

            string targetPath = folderPath;

            while (!_folderIndex.ContainsKey(targetPath) && targetPath.Length != 0)
            {
                targetPath = AssetFolderViewModel.GetParentPath(targetPath);
            }

            SetCurrentFolder(_folderIndex[targetPath]);

            SelectedItem = previousSelection switch
            {
                AssetObjectViewModel asset => Items.OfType<AssetObjectViewModel>().FirstOrDefault(item => item.Bucket?.BucketIndex == asset.Bucket?.BucketIndex && item.AssetDesc.Name == asset.AssetDesc.Name),
                AssetFolderViewModel folder => Items.OfType<AssetFolderViewModel>().FirstOrDefault(item => item.Path == folder.Path),
                _ => null
            };

            ResolvePendingFocus();
            ResolvePendingRename();
        }

        private void RebuildFolderTree(HashSet<string> expandedPaths)
        {
            _folderIndex.Clear();

            RootFolder = new AssetFolderViewModel(string.Empty, null) { IsExpanded = expandedPaths.Contains(string.Empty) };
            _folderIndex[string.Empty] = RootFolder;

            RootFolders.Clear();
            RootFolders.Add(RootFolder);

            if (_assetFolders == null)
            {
                return;
            }

            List<string> folderPaths = _assetFolders.GetFolderPaths().Cast<Name>()
                .Select(name => name.ToString())
                .Where(path => path.Length != 0)
                .OrderBy(path => path.Count(ch => ch == '/'))
                .ThenBy(path => path, StringComparer.OrdinalIgnoreCase)
                .ToList();

            foreach (string path in folderPaths)
            {
                if (_folderIndex.ContainsKey(path))
                {
                    continue;
                }

                AssetFolderViewModel parent = GetOrCreateFolder(AssetFolderViewModel.GetParentPath(path), expandedPaths);
                AssetFolderViewModel folder = new AssetFolderViewModel(path, parent) { IsExpanded = expandedPaths.Contains(path) };

                parent.Children.Add(folder);
                _folderIndex[path] = folder;
            }
        }

        private AssetFolderViewModel GetOrCreateFolder(string path, HashSet<string> expandedPaths)
        {
            if (_folderIndex.TryGetValue(path, out AssetFolderViewModel? existing))
            {
                return existing;
            }

            AssetFolderViewModel parent = GetOrCreateFolder(AssetFolderViewModel.GetParentPath(path), expandedPaths);
            AssetFolderViewModel folder = new AssetFolderViewModel(path, parent) { IsExpanded = expandedPaths.Contains(path) };

            parent.Children.Add(folder);
            _folderIndex[path] = folder;

            return folder;
        }

        private void RebuildAssetIndex()
        {
            _thumbnailService.CancelPending();
            _thumbnailService.ClearSubscribers();

            _allAssets.Clear();

            if (_assetFolders == null)
            {
                return;
            }

            AssetRegistry registry = AssetManager.Instance.AssetRegistry;

            foreach (AssetBucketViewModel bucketVm in _buckets.Values)
            {
                foreach (AssetDesc assetDesc in registry.GetBucketAssetDescs(bucketVm.BucketIndex))
                {
                    string folderPath = _assetFolders.GetAssetFolderOf(bucketVm.BucketIndex, assetDesc.Name);

                    AssetObjectViewModel assetVm = new AssetObjectViewModel(assetDesc, bucketVm) { FolderPath = folderPath };
                    _allAssets.Add(assetVm);

                    if (_folderIndex.TryGetValue(folderPath, out AssetFolderViewModel? folder))
                    {
                        folder.AssetCount++;
                    }
                }
            }
        }

        private void SetCurrentFolder(AssetFolderViewModel folder)
        {
            _currentFolder = folder;

            string folderPath = folder.Path;
            _ = EngineManager.PostToSimThread(() => _editorSubsystem.SetCurrentAssetFolder(folderPath));

            for (AssetFolderViewModel? ancestor = folder.Parent; ancestor != null; ancestor = ancestor.Parent)
            {
                ancestor.IsExpanded = true;
            }

            PathSegments.Clear();

            for (AssetFolderViewModel? segment = folder; segment != null; segment = segment.Parent)
            {
                PathSegments.Insert(0, segment);
            }

            OnPropertyChanged(nameof(CurrentFolder));
            OnPropertyChanged(nameof(SelectedTreeFolder));
            OnPropertyChanged(nameof(SearchWatermark));

            (GoBackCommand as RelayCommand)?.RaiseCanExecuteChanged();
            (GoForwardCommand as RelayCommand)?.RaiseCanExecuteChanged();
            (GoUpCommand as RelayCommand)?.RaiseCanExecuteChanged();

            RebuildItems();
        }

        public void NavigateTo(AssetFolderViewModel folder)
        {
            Dispatcher.UIThread.VerifyAccess();

            if (ReferenceEquals(folder, _currentFolder))
            {
                return;
            }

            _backHistory.Push(_currentFolder.Path);
            _forwardHistory.Clear();

            SelectedItem = null;
            SetCurrentFolder(folder);
        }

        public void NavigateToPath(string folderPath)
        {
            if (_folderIndex.TryGetValue(folderPath, out AssetFolderViewModel? folder))
            {
                NavigateTo(folder);
            }
        }

        private void GoBack()
        {
            NavigateHistory(_backHistory, _forwardHistory);
        }

        private void GoForward()
        {
            NavigateHistory(_forwardHistory, _backHistory);
        }

        private void NavigateHistory(Stack<string> from, Stack<string> to)
        {
            while (from.Count > 0)
            {
                string path = from.Pop();

                if (_folderIndex.TryGetValue(path, out AssetFolderViewModel? folder))
                {
                    to.Push(_currentFolder.Path);

                    // the folder just left stays selected, so stepping back out of a folder keeps your place
                    AssetFolderViewModel previousFolder = _currentFolder;

                    SetCurrentFolder(folder);

                    SelectedItem = Items.OfType<AssetFolderViewModel>().FirstOrDefault(item => ReferenceEquals(item, previousFolder));

                    return;
                }
            }

            (GoBackCommand as RelayCommand)?.RaiseCanExecuteChanged();
            (GoForwardCommand as RelayCommand)?.RaiseCanExecuteChanged();
        }

        private void GoUp()
        {
            if (_currentFolder.Parent is not { } parent)
            {
                return;
            }

            AssetFolderViewModel previousFolder = _currentFolder;

            NavigateTo(parent);

            SelectedItem = previousFolder;
        }

        private void OnTypeFilterChanged()
        {
            OnPropertyChanged(nameof(HasTypeFilter));
            RebuildItems();
        }

        private void RebuildItems()
        {
            Dispatcher.UIThread.VerifyAccess();

            object? preserved = _selectedItem;

            HashSet<uint>? typeFilter = HasTypeFilter
                ? [.. TypeFilters.Where(filter => filter.IsChecked).Select(filter => filter.BucketIndex)]
                : null;

            IEnumerable<AssetFolderViewModel> folders;
            IEnumerable<AssetObjectViewModel> assets;

            if (HasSearchText)
            {
                folders = _folderIndex.Values.Where(folder =>
                    !ReferenceEquals(folder, _currentFolder)
                    && folder.IsSameOrDescendantOf(_currentFolder)
                    && folder.Name.Contains(_searchText, StringComparison.OrdinalIgnoreCase));

                assets = _allAssets.Where(asset =>
                    AssetFolderViewModel.IsSameOrDescendantPath(asset.FolderPath, _currentFolder.Path)
                    && asset.DisplayName.Contains(_searchText, StringComparison.OrdinalIgnoreCase));
            }
            else
            {
                folders = _currentFolder.Children;
                assets = _allAssets.Where(asset => string.Equals(asset.FolderPath, _currentFolder.Path, StringComparison.Ordinal));
            }

            if (typeFilter != null)
            {
                folders = Enumerable.Empty<AssetFolderViewModel>();
                assets = assets.Where(asset => asset.Bucket != null && typeFilter.Contains(asset.Bucket.BucketIndex));
            }

            List<AssetObjectViewModel> assetList = assets.ToList();

            foreach (AssetObjectViewModel asset in assetList)
            {
                asset.EnsureFileInfo();
            }

            List<AssetObjectViewModel> sortedAssets = _sortMode switch
            {
                AssetSortMode.DateModified =>
                    assetList.OrderByDescending(a => a.DateModified ?? DateTime.MinValue).ToList(),
                AssetSortMode.Type =>
                    assetList.OrderBy(a => a.TypeName ?? string.Empty, StringComparer.OrdinalIgnoreCase)
                            .ThenBy(a => a.DisplayName, StringComparer.OrdinalIgnoreCase)
                            .ToList(),
                _ /* Name */ =>
                    assetList.OrderBy(a => a.DisplayName, StringComparer.OrdinalIgnoreCase).ToList(),
            };

            Items.Clear();

            foreach (AssetFolderViewModel folder in folders.OrderBy(folder => folder.Name, StringComparer.OrdinalIgnoreCase))
            {
                Items.Add(folder);
            }

            foreach (AssetObjectViewModel asset in sortedAssets)
            {
                Items.Add(asset);
            }

            // The filter may have just hidden whatever was selected.
            SelectedItem = preserved != null && Items.Contains(preserved) ? preserved : null;

            OnPropertyChanged(nameof(HasNoMatches));
            OnPropertyChanged(nameof(IsFolderEmpty));

            ResolveUnsavedAssetTypeNames(sortedAssets.Where(asset => !asset.HasManifest).ToList());

            foreach (AssetObjectViewModel asset in sortedAssets)
            {
                asset.RequestThumbnail();
            }
        }

        private static void PushFolderAction(string text, Action execute, Action revert)
        {
            _ = EngineManager.PostToSimThread(() =>
            {
                EditorProject? project = EngineManager.CurrentProject;

                if (project?.ActionStack == null)
                {
                    execute();
                    return;
                }

                project.ActionStack.PushAction(new EditorAction(text, (_, _) => execute(), (_, _) => revert()));
            });
        }

        /// <summary>Makes a new folder under <paramref name="parent"/> and prepares the name to be edited</summary>
        public void CreateFolder(AssetFolderViewModel parent)
        {
            EditorAssetFolders? assetFolders = _assetFolders;

            if (assetFolders == null)
            {
                return;
            }

            string path = AssetFolderViewModel.JoinPath(parent.Path, assetFolders.MakeUniqueFolderName(parent.Path, "New Folder"));

            _pendingRenamePath = path;

            if (!ReferenceEquals(parent, _currentFolder))
            {
                parent.IsExpanded = true;
            }

            PushFolderAction("New Folder",
                execute: () => assetFolders.CreateFolder(path),
                revert: () => assetFolders.RemoveFolder(path));
        }

        public void RenameFolder(AssetFolderViewModel folder, string newName)
        {
            EditorAssetFolders? assetFolders = _assetFolders;

            folder.CancelRename();

            newName = newName.Trim();

            if (assetFolders == null || folder.IsRoot || newName.Length == 0 || newName == folder.Name || newName.IndexOfAny(new[] { '/', '\\', '|', '\t', '\n', '\r' }) >= 0)
            {
                return;
            }

            string oldPath = folder.Path;
            string oldName = folder.Name;
            string newPath = oldPath;

            PushFolderAction($"Rename {oldName}",
                execute: () => newPath = assetFolders.RenameFolder(oldPath, newName) is { Length: > 0 } renamed ? renamed : oldPath,
                revert: () => assetFolders.RenameFolder(newPath, oldName));
        }

        public void RenameAsset(AssetObjectViewModel asset, string newName)
        {
            asset.CancelRename();

            newName = newName.Trim();

            if (asset.Bucket == null || newName.Length == 0)
            {
                return;
            }

            AssetRegistry registry = AssetManager.Instance.AssetRegistry;

            newName = registry.SanitizeAssetName(newName).ToString();

            if (newName == asset.DisplayName)
            {
                return;
            }

            uint bucketIndex = asset.Bucket.BucketIndex;
            string oldName = asset.DisplayName;

            // ignoring case, as the manifests would collide on a case-insensitive file system.
            bool isNameTaken = registry.GetBucketAssetDescs(bucketIndex)
                .Select(assetDesc => assetDesc.Name.ToString())
                .Any(existingName => existingName != oldName && string.Equals(existingName, newName, StringComparison.OrdinalIgnoreCase));

            if (isNameTaken)
            {
                Logger.Log(LogLevel.Warning, $"Cannot rename {oldName}: an asset named {newName} already exists in {asset.Bucket.Name}.");
                return;
            }

            _pendingFocusBucket = bucketIndex;
            _pendingFocusName = newName;

            _editorSubsystem.ExecuteCommandByName(new Name("EditorCommandRenameAsset"), $"{bucketIndex} {oldName} {newName}");
        }

        public void MoveItems(IReadOnlyList<object> items, AssetFolderViewModel target)
        {
            EditorAssetFolders? assetFolders = _assetFolders;

            if (assetFolders == null)
            {
                return;
            }

            List<AssetFolderViewModel> folders = items.OfType<AssetFolderViewModel>()
                .Where(folder => !folder.IsRoot && !target.IsSameOrDescendantOf(folder) && !ReferenceEquals(folder.Parent, target))
                .ToList();

            // a folder inside another moved folder travels with it
            folders = folders.Where(folder => !folders.Any(other => !ReferenceEquals(other, folder) && folder.IsSameOrDescendantOf(other))).ToList();

            List<AssetObjectViewModel> assets = items.OfType<AssetObjectViewModel>()
                .Where(asset => asset.Bucket != null
                    && asset.FolderPath != target.Path
                    && !folders.Any(folder => AssetFolderViewModel.IsSameOrDescendantPath(asset.FolderPath, folder.Path)))
                .ToList();

            if (folders.Count == 0 && assets.Count == 0)
            {
                return;
            }

            var folderMoves = folders.Select(folder => (OldPath: folder.Path, OldParent: AssetFolderViewModel.GetParentPath(folder.Path), OldName: folder.Name)).ToList();
            var assetMoves = assets.Select(asset => (BucketIndex: asset.Bucket!.BucketIndex, Name: asset.AssetDesc.Name, OldFolder: asset.FolderPath)).ToList();

            string targetPath = target.Path;
            string[] movedPaths = new string[folderMoves.Count];

            int count = folderMoves.Count + assetMoves.Count;
            string text = count == 1
                ? $"Move {(folderMoves.Count == 1 ? folderMoves[0].OldName : assetMoves[0].Name.ToString())}"
                : $"Move {count} items";

            PushFolderAction(text,
                execute: () =>
                {
                    for (int i = 0; i < folderMoves.Count; i++)
                    {
                        movedPaths[i] = assetFolders.MoveFolder(folderMoves[i].OldPath, targetPath);
                    }

                    foreach (var assetMove in assetMoves)
                    {
                        assetFolders.SetAssetFolderOf(assetMove.BucketIndex, assetMove.Name, targetPath);
                    }
                },
                revert: () =>
                {
                    foreach (var assetMove in assetMoves)
                    {
                        assetFolders.SetAssetFolderOf(assetMove.BucketIndex, assetMove.Name, assetMove.OldFolder);
                    }

                    for (int i = folderMoves.Count - 1; i >= 0; i--)
                    {
                        if (string.IsNullOrEmpty(movedPaths[i]))
                        {
                            continue;
                        }

                        string restoredPath = assetFolders.MoveFolder(movedPaths[i], folderMoves[i].OldParent);

                        if (restoredPath.Length != 0 && AssetFolderViewModel.GetFolderName(restoredPath) != folderMoves[i].OldName)
                        {
                            assetFolders.RenameFolder(restoredPath, folderMoves[i].OldName);
                        }
                    }
                });
        }

        public void DeleteItems(IReadOnlyList<object> items)
        {
            List<AssetObjectViewModel> assets = items.OfType<AssetObjectViewModel>().Where(asset => asset.Bucket != null).ToList();
            List<AssetFolderViewModel> folders = items.OfType<AssetFolderViewModel>().Where(folder => !folder.IsRoot).ToList();

            // whatever is inside a deleted folder goes with it
            assets = assets.Where(asset => !folders.Any(folder => AssetFolderViewModel.IsSameOrDescendantPath(asset.FolderPath, folder.Path))).ToList();
            folders = folders.Where(folder => !folders.Any(other => !ReferenceEquals(other, folder) && folder.IsSameOrDescendantOf(other))).ToList();

            List<string> folderPaths = folders.Select(folder => folder.Path).ToList();
            string assetArgs = string.Join(" ", assets.Select(asset => $"{asset.Bucket!.BucketIndex} {asset.AssetDesc.Name}"));

            _ = EngineManager.PostToSimThread(() =>
            {
                foreach (string folderPath in folderPaths)
                {
                    _editorSubsystem.ExecuteCommandByName(new Name("EditorCommandDeleteAssetFolder"), folderPath);
                }

                if (assetArgs.Length != 0)
                {
                    _editorSubsystem.ExecuteCommandByName(new Name("EditorCommandDeleteAsset"), assetArgs);
                }
            });
        }

        public void ImportFiles(IReadOnlyList<string> paths, AssetFolderViewModel folder)
        {
            if (paths.Count == 0)
            {
                return;
            }

            string args = folder.Path + "|" + string.Join("|", paths);

            _ = EngineManager.PostToSimThread(() =>
            {
                if (!CanCreateAssetsOnSimThread("import"))
                {
                    return;
                }

                _editorSubsystem.ExecuteCommandByName(new Name("EditorCommandImportFiles"), args);
            });
        }

        private void ResolvePendingRename()
        {
            if (_pendingRenamePath == null || !_folderIndex.TryGetValue(_pendingRenamePath, out AssetFolderViewModel? folder))
            {
                return;
            }

            _pendingRenamePath = null;

            AssetFolderViewModel renameTarget = Items.OfType<AssetFolderViewModel>().FirstOrDefault(item => ReferenceEquals(item, folder)) ?? folder;

            SelectedItem = Items.Contains(renameTarget) ? renameTarget : SelectedItem;

            renameTarget.BeginRename();
            FolderRenameRequested?.Invoke(renameTarget);
        }

        private string MakeUniqueAssetName(uint bucketIndex, string baseName)
        {
            AssetRegistry registry = AssetManager.Instance.AssetRegistry;

            var existingNames = new HashSet<string>(
                registry.GetBucketAssetDescs(bucketIndex).Select(assetDesc => assetDesc.Name.ToString()),
                StringComparer.Ordinal);

            string name = baseName;

            for (int suffix = 1; existingNames.Contains(name); suffix++)
            {
                name = $"{baseName}_{suffix}";
            }

            return name;
        }

        public async Task<IReadOnlyList<AssetCreationEntry>> GetCreatableAssetEntriesAsync()
        {
            Dispatcher.UIThread.VerifyAccess();

            List<CreatableAssetType> types = await EngineManager.PostToSimThread<List<CreatableAssetType>>(BuildCreatableAssetTypes);

            List<AssetCreationEntry> entries = new();
            foreach (var kvp in DedicatedCreatorTypes)
            {
                entries.Add(new AssetCreationEntry(kvp.Key, kvp.Value(this)));
            }

            foreach (CreatableAssetType type in types)
            {
                ICommand command = type.PickerEntries is { } pickerEntries
                    ? new RelayCommand(() => OpenAssetTypePicker(pickerEntries), () => CanCreateAssets)
                    : new RelayCommand(() => CreateAssetOfClass(type.Name), () => CanCreateAssets);

                entries.Add(new AssetCreationEntry(type.Name, command, type.IsUserDefined));
            }

            // Engine types first, then UDT
            return entries
                .OrderBy(entry => entry.IsUserDefined)
                .ThenBy(entry => entry.Header, StringComparer.OrdinalIgnoreCase)
                .ToList();
        }

        private static List<CreatableAssetType> BuildCreatableAssetTypes()
        {
            Class? assetClass = Class.TryGetClass<AssetObject>();

            if (!assetClass.HasValue)
            {
                return [];
            }

            HashSet<string> dedicatedNames = new HashSet<string>(StringComparer.Ordinal);

            foreach (var kvp in DedicatedCreatorTypes)
            {
                Class? dedicatedClass = Class.TryGetClass(kvp.Key);

                if (!dedicatedClass.HasValue)
                {
                    continue;
                }

                dedicatedNames.Add(dedicatedClass.Value.Name.ToString());

                foreach (ClassPickerEntry derived in ClassPickerEntries.BuildDerived(dedicatedClass.Value.TypeId))
                {
                    dedicatedNames.Add(derived.Name);
                }
            }

            HashSet<string> pickerNames = new(StringComparer.Ordinal);
            List<CreatableAssetType> types = new();

            foreach (ClassPickerEntry entry in ClassPickerEntries.BuildDerived(assetClass.Value.TypeId))
            {
                if (dedicatedNames.Contains(entry.Name)
                    || pickerNames.Contains(entry.Name)
                    || Class.TryGetClass(entry.Name) is not { } cls)
                {
                    continue;
                }

                if (entry.IsAbstract)
                {
                    IReadOnlyList<ClassPickerEntry> pickerEntries = ClassPickerEntries.BuildCreatableEntries(cls);

                    if (!pickerEntries.Any(pickerEntry => !pickerEntry.IsAbstract))
                    {
                        continue;
                    }

                    types.Add(new CreatableAssetType(entry.Name, cls.IsUserDefined, pickerEntries));

                    foreach (ClassPickerEntry pickerEntry in pickerEntries)
                    {
                        pickerNames.Add(pickerEntry.Name);
                    }

                    continue;
                }

                if (ClassPickerEntries.CanCreateAssetForType(cls))
                {
                    types.Add(new CreatableAssetType(entry.Name, cls.IsUserDefined, null));
                }
            }

            return types;
        }

        private void OpenAssetTypePicker(IReadOnlyList<ClassPickerEntry> entries)
        {
            try
            {
                string folderPath = _currentFolder.Path;

                PanelService.Instance.OpenPanel(new NewAssetPanelViewModel(entries, CreatePendingAsset, asset =>
                {
                    if (asset == null)
                    {
                        return;
                    }

                    _ = EngineManager.PostToSimThread(() => CommitPendingAsset(asset, folderPath));
                }));
            }
            catch (Exception ex)
            {
                Logger.Log(LogLevel.Error, $"Failed to open the new asset panel: {ex}");
            }
        }

        private static AssetObject? CreatePendingAsset(string className)
        {
            if (!NativeBindings.Hyp_CreateInstanceOfClass(className, out BoxedValueInternal result))
            {
                Logger.Log(LogLevel.Warning, $"Failed to create instance of class '{className}'");
                return null;
            }

            using BoxedValue boxed = BoxedValue.FromBuffer(result);

            return boxed.GetValue() as AssetObject;
        }

        private void CommitPendingAsset(AssetObject asset, string folderPath)
        {
            if (!CanCreateAssetsOnSimThread(asset.Class.Name.ToString()))
            {
                return;
            }

            AssetRegistry registry = AssetManager.Instance.AssetRegistry;
            registry.PutAssetUnique(asset);

            if (!asset.IsRegistered())
            {
                Logger.Log(LogLevel.Warning, $"New '{asset.Class.Name}' could not be added to the asset registry");
                return;
            }

            uint bucketIndex = asset.Path.BucketIndex;
            Name assetName = asset.Name;

            EngineManager.CurrentProject?.AssetFolders?.SetAssetFolderOf(bucketIndex, assetName, folderPath);
            NotifyAssetsChanged();

            string focusName = assetName.ToString();
            Dispatcher.UIThread.Post(() => FocusAsset(bucketIndex, focusName, beginRename: true));
        }

        private void CreateAssetOfClass(string className)
        {
            string folderPath = _currentFolder.Path;

            _ = EngineManager.PostToSimThread(() =>
            {
                if (!CanCreateAssetsOnSimThread(className))
                {
                    return;
                }

                HashSet<(uint BucketIndex, string Name)> assetsBefore = SnapshotAssets()
                    .Select(asset => (asset.BucketIndex, asset.Name.ToString()))
                    .ToHashSet();

                _editorSubsystem.ExecuteCommandByName(new Name("EditorCommandNewAsset"), $"{className} New{className}");

                List<(uint BucketIndex, Name Name)> created = SnapshotAssets()
                    .Where(asset => !assetsBefore.Contains((asset.BucketIndex, asset.Name.ToString())))
                    .ToList();

                if (created.Count == 0)
                {
                    Logger.Log(LogLevel.Error, $"New {className} creation failed; no new asset appeared in the registry.");
                    return;
                }

                EditorAssetFolders? assetFolders = EngineManager.CurrentProject?.AssetFolders;

                foreach ((uint bucketIndex, Name name) in created)
                {
                    assetFolders?.SetAssetFolderOf(bucketIndex, name, folderPath);
                }

                NotifyAssetsChanged();

                (uint focusBucket, Name focusName) = created[0];
                Dispatcher.UIThread.Post(() => FocusAsset(focusBucket, focusName.ToString(), beginRename: true));
            });
        }

        private static List<(uint BucketIndex, Name Name)> SnapshotAssets()
        {
            AssetRegistry registry = AssetManager.Instance.AssetRegistry;

            List<(uint BucketIndex, Name Name)> assets = new List<(uint BucketIndex, Name Name)>();

            foreach (AssetBucket bucket in AssetBucket.AllBuckets)
            {
                if (bucket.Value == AssetBucket.InvalidIndex)
                {
                    continue;
                }

                foreach (AssetDesc assetDesc in registry.GetBucketAssetDescs(bucket.Value))
                {
                    assets.Add((bucket.Value, assetDesc.Name));
                }
            }

            return assets;
        }

        private void CreateInCurrentFolder(string assetDescription, uint bucketIndex, bool openEditor, Action create)
        {
            string folderPath = _currentFolder.Path;

            _ = EngineManager.PostToSimThread(() =>
            {
                if (!CanCreateAssetsOnSimThread(assetDescription))
                {
                    return;
                }

                AssetRegistry registry = AssetManager.Instance.AssetRegistry;

                var namesBefore = new HashSet<string>(
                    registry.GetBucketAssetDescs(bucketIndex).Select(assetDesc => assetDesc.Name.ToString()),
                    StringComparer.Ordinal);

                create();

                List<Name> createdNames = registry.GetBucketAssetDescs(bucketIndex)
                    .Select(assetDesc => assetDesc.Name)
                    .Where(name => !namesBefore.Contains(name.ToString()))
                    .ToList();

                if (createdNames.Count == 0)
                {
                    Logger.Log(LogLevel.Error, $"New {assetDescription} creation failed; no new asset appeared in the {AssetBucket.GetAssetBucketName(bucketIndex)} bucket.");
                    return;
                }

                EditorAssetFolders? assetFolders = EngineManager.CurrentProject?.AssetFolders;

                foreach (Name createdName in createdNames)
                {
                    assetFolders?.SetAssetFolderOf(bucketIndex, createdName, folderPath);
                }

                string focusName = createdNames[0].ToString();

                Dispatcher.UIThread.Post(() => FocusAsset(bucketIndex, focusName, openEditor));
            });
        }

        private static void ResolveUnsavedAssetTypeNames(List<AssetObjectViewModel> assetVms)
        {
            if (assetVms.Count == 0)
            {
                return;
            }

            _ = EngineManager.PostToSimThread(() =>
            {
                AssetRegistry registry = AssetManager.Instance.AssetRegistry;

                List<(AssetObjectViewModel AssetVm, string TypeName)> resolved = new List<(AssetObjectViewModel, string)>();

                foreach (AssetObjectViewModel assetVm in assetVms)
                {
                    if (assetVm.Bucket == null)
                    {
                        continue;
                    }

                    AssetObject? obj = registry.GetAsset(assetVm.Bucket.BucketIndex, assetVm.AssetDesc.Name);

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

        public void Dispose()
        {
            SaveBrowserState();

            _thumbnailService.Dispose();

            _onAssetsChangedHandler?.Remove();
            _onAssetsChangedHandler?.Dispose();
            _onFoldersChangedHandler?.Remove();
            _onFoldersChangedHandler?.Dispose();
            _onProjectOpenedHandler?.Remove();
            _onProjectOpenedHandler?.Dispose();
            _onProjectClosingHandler?.Remove();
            _onProjectClosingHandler?.Dispose();
        }

        public void NotifyAssetsChanged()
        {
            // on the sim thread, so a command that just created an asset has filed it before unfiled ones get their default folder
            _ = EngineManager.PostToSimThread(() =>
            {
                _assetFolders?.SyncWithRegistry();

                Dispatcher.UIThread.Post(() =>
                {
                    // An asset was added, removed or edited - the decoded images we are holding may no longer match what is on disk.
                    _thumbnailService.Clear();

                    QueueReload();
                });
            });
        }

        public void FocusAsset(uint bucketIndex, string name, bool openEditor = false, bool beginRename = false)
        {
            Dispatcher.UIThread.VerifyAccess();

            if (bucketIndex == 0)
            {
                return;
            }

            BringToFrontRequested?.Invoke();

            // An active search or type filter would hide the asset we are about to select.
            SearchText = string.Empty;
            ClearTypeFilterCommand.Execute(null);

            _pendingFocusBucket = bucketIndex;
            _pendingFocusName = name;
            _pendingOpenEditor = openEditor;
            _pendingBeginRename = beginRename;

            QueueReload();
        }

        private void ResolvePendingFocus()
        {
            if (_pendingFocusBucket == 0 || _pendingFocusName == null)
            {
                return;
            }

            AssetObjectViewModel? asset = _allAssets.FirstOrDefault(a =>
                a.Bucket?.BucketIndex == _pendingFocusBucket
                && string.Equals(a.DisplayName, _pendingFocusName, StringComparison.Ordinal));

            if (asset == null)
            {
                return;
            }

            bool openEditor = _pendingOpenEditor;
            bool beginRename = _pendingBeginRename;

            _pendingFocusBucket = 0;
            _pendingFocusName = null;
            _pendingOpenEditor = false;
            _pendingBeginRename = false;

            if (_folderIndex.TryGetValue(asset.FolderPath, out AssetFolderViewModel? folder) && !ReferenceEquals(folder, _currentFolder))
            {
                NavigateTo(folder);
            }

            SelectedItem = Items.OfType<AssetObjectViewModel>().FirstOrDefault(item => ReferenceEquals(item, asset));

            if (openEditor)
            {
                OpenAssetEditor(asset);
            }

            if (beginRename)
            {
                asset.BeginRename();
                AssetRenameRequested?.Invoke(asset);
            }
        }

        public void OpenItem(object? item)
        {
            switch (item)
            {
                case AssetFolderViewModel folder:
                    NavigateTo(folder);
                    break;
                case AssetObjectViewModel asset when asset.IsWorld:
                    OpenWorldCommand.Execute(asset);
                    break;
                case AssetObjectViewModel asset:
                    EditAssetCommand.Execute(asset);
                    break;
            }
        }

        /// <summary>Opens the asset in a pop-out property editor panel, the same one used by the "Edit" button on asset-object properties in the inspector. Works for any AssetObject-derived type. Prefabs open in prefab edit mode instead.</summary>
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

                if (obj is Prefab)
                {
                    Dispatcher.UIThread.Post(() => MainWindowViewModel.Instance?.RequestEditPrefab(assetName.ToString()));

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
