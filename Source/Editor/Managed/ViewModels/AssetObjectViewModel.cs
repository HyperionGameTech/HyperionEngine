using System;
using System.Collections.ObjectModel;
using System.Threading;
using Avalonia.Media;
using Avalonia.Threading;
using Hyperion;
using Hyperion.Editor.Services;

namespace Hyperion.Editor.ViewModels
{
    public class AssetObjectViewModel : ViewModelBase
    {
        private readonly AssetDesc _assetDesc;
        private readonly AssetBucketViewModel? _bucket;
        private string? _typeName;
        private readonly DateTime? _dateModified;

        public AssetDesc AssetDesc => _assetDesc;
        public AssetBucketViewModel? Bucket => _bucket;

        public string DisplayName => _assetDesc.Name.ToString();

        public string ToolTipText => _bucket != null
            ? $"{DisplayName}\n{_bucket.Name}"
            : DisplayName;

        /// <summary>Asset class name extracted from the manifest (e.g. "MeshAsset"), or null if unavailable.</summary>
        public string? TypeName => _typeName;

        /// <summary>Last-write time of the manifest file on disk, or null if unavailable.</summary>
        public DateTime? DateModified => _dateModified;

        // @TODO also, dont just switch on typename -- switch on class -- we dont want to add a new entry for EVERY subclass !
        public string IconKind => _typeName != null
            ? AssetIconHelper.FromTypeName(_typeName)
            : "File";

        /// <summary>Fills in the type for an asset that has no manifest on disk yet (created but not saved).</summary>
        public void SetTypeName(string typeName)
        {
            Dispatcher.UIThread.VerifyAccess();

            _typeName = typeName;

            OnPropertyChanged(nameof(TypeName));
            OnPropertyChanged(nameof(IconKind));
        }

        private IImage? _thumbnail;

        /// <summary>Rendered preview of this asset, or null while none has been generated. The type icon stands in until it arrives.</summary>
        public IImage? Thumbnail
        {
            get => _thumbnail;
            private set
            {
                if (SetProperty(ref _thumbnail, value))
                {
                    OnPropertyChanged(nameof(HasThumbnail));
                }
            }
        }

        public bool HasThumbnail => _thumbnail != null;

        private int _isRequestingThumbnail;

        /// <summary>Asks for this asset's preview image, if one hasn't already been requested for this view model.</summary>
        public void RequestThumbnail()
        {
            Dispatcher.UIThread.VerifyAccess();

            if (ThumbnailService.Instance == null || _thumbnail != null)
            {
                return;
            }

            if (_bucket == null)
            {
                return;
            }

            if (Interlocked.Exchange(ref _isRequestingThumbnail, 1) != 0)
            {
                return;
            }

            ThumbnailService.Instance.Request(_bucket.BucketIndex, _assetDesc.Name, image =>
            {
                _isRequestingThumbnail = 0;

                Thumbnail = image;
            });
        }

        public ObservableCollection<InspectorActionViewModel> Actions { get; } = new ObservableCollection<InspectorActionViewModel>();

        private bool _hasActions;
        public bool HasActions
        {
            get => _hasActions;
            private set => SetProperty(ref _hasActions, value);
        }

        private int _isRefreshingActions;

        public AssetObjectViewModel(AssetDesc assetDesc, AssetBucketViewModel? bucket = null, string? typeName = null, DateTime? dateModified = null)
        {
            _assetDesc = assetDesc;
            _bucket = bucket;
            _typeName = typeName;
            _dateModified = dateModified;
        }

        /// <summary>Resolves the live asset object on the sim thread and repopulates <see cref="Actions"/> from its EditorAction-attributed methods (e.g. "Regenerate Mipmaps" on Texture).</summary>
        public void RefreshActions()
        {
            Dispatcher.UIThread.VerifyAccess();

            if (_bucket == null)
            {
                return;
            }

            if (Interlocked.Exchange(ref _isRefreshingActions, 1) != 0)
            {
                return;
            }

            uint bucketIndex = _bucket.BucketIndex;
            Name assetName = _assetDesc.Name;

            _ = EngineManager.PostToSimThread(() =>
            {
                AssetObject? obj = AssetManager.Instance.AssetRegistry.GetAsset(bucketIndex, assetName);

                List<InspectorActionViewModel> actionVms = InspectorActionsHelper.GetActionsOnSimThread(obj);

                Dispatcher.UIThread.Post(() =>
                {
                    _isRefreshingActions = 0;

                    Actions.Clear();

                    foreach (InspectorActionViewModel actionVm in actionVms)
                    {
                        Actions.Add(actionVm);
                    }

                    HasActions = Actions.Count > 0;
                });
            });
        }
    }
}
