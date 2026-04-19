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

        private DelegateHandler? _onSelectedBucketChangedHandler;

        public ICommand ImportCommand { get; }

        public ContentBrowserViewModel(EditorSubsystem editorSubsystem)
        {
            _editorSubsystem = editorSubsystem ?? throw new ArgumentNullException(nameof(editorSubsystem));

            ImportCommand = new EditorCommand("ImportContent");

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

                            foreach (AssetDesc assetDesc in registry.GetBucketAssetDescs(bucketIndex))
                            {
                                Assets.Add(new AssetObjectViewModel(assetDesc, bucketVm));
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

                    OnPropertyChanged(nameof(Assets));
                    OnPropertyChanged(nameof(CurrentBucket));
                });
            });
        }

        public void Dispose()
        {
            _onSelectedBucketChangedHandler?.Remove();
            _onSelectedBucketChangedHandler?.Dispose();
        }
    }
}
