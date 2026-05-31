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

        // ── sort ──────────────────────────────────────────────────────────────

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

        // ─────────────────────────────────────────────────────────────────────

        private DelegateHandler? _onSelectedBucketChangedHandler;
        private uint _pendingFocusBucket;
        private string? _pendingFocusNameHint;

        public ICommand ImportCommand { get; }
        public ICommand NewScriptCommand { get; }

        public ContentBrowserViewModel(EditorSubsystem editorSubsystem)
        {
            _editorSubsystem = editorSubsystem ?? throw new ArgumentNullException(nameof(editorSubsystem));

            ImportCommand = new EditorCommand("ImportContent");
            NewScriptCommand = new RelayCommand(() =>
            {
                _editorSubsystem.ExecuteCommandByName(new Name("EditorCommandNewScript"));
                FocusAsset(AssetBucket.Scripts.Value, "NewScript");
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

                Dispatcher.UIThread.Post(() =>
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
                                string? typeName = null;

                                if (File.Exists(manifestPath))
                                {
                                    dateModified = File.GetLastWriteTime(manifestPath);
                                    typeName = ExtractClassFromManifest(manifestPath);
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
                    }

                    OnPropertyChanged(nameof(Assets));
                    OnPropertyChanged(nameof(CurrentBucket));
                });
            });
        }

        // ── sort helpers ──────────────────────────────────────────────────────

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

        /// <summary>
        /// Quick-reads a manifest JSON file and extracts the value of the "$Class" key
        /// without fully parsing the document. Returns null if not found or on any error.
        /// </summary>
        private static string? ExtractClassFromManifest(string filePath)
        {
            try
            {
                using var reader = new StreamReader(filePath);

                // Read the first 4 KB — always enough to find "$Class" near the top.
                char[] buffer = new char[4096];
                int read = reader.ReadBlock(buffer, 0, buffer.Length);

                if (read <= 0)
                    return null;

                string header = new string(buffer, 0, read);
                int classIdx = header.IndexOf("\"$Class\"", StringComparison.Ordinal);

                if (classIdx < 0)
                    return null;

                int colonIdx = header.IndexOf(':', classIdx + 8);

                if (colonIdx < 0)
                    return null;

                int quoteStart = header.IndexOf('"', colonIdx + 1);

                if (quoteStart < 0)
                    return null;

                int quoteEnd = header.IndexOf('"', quoteStart + 1);

                if (quoteEnd <= quoteStart)
                    return null;

                return header.Substring(quoteStart + 1, quoteEnd - quoteStart - 1);
            }
            catch
            {
                return null;
            }
        }

        public void Dispose()
        {
            _onSelectedBucketChangedHandler?.Remove();
            _onSelectedBucketChangedHandler?.Dispose();
        }

        /// <summary>Switches to the given bucket and focuses the named asset once loaded.</summary>
        public void FocusAsset(uint bucketIndex, string? nameHint = null)
        {
            Dispatcher.UIThread.VerifyAccess();

            if (bucketIndex == 0)
                return;

            _pendingFocusBucket = bucketIndex;
            _pendingFocusNameHint = nameHint;
            _editorSubsystem.SetSelectedBucket(bucketIndex);
        }
    }
}
