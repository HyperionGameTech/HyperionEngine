using System;
using System.Collections.Generic;
using System.Collections.ObjectModel;
using System.IO;
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
        private DateTime? _dateModified;
        private bool _hasFileInfo;
        private bool _hasManifest;

        public AssetDesc AssetDesc => _assetDesc;
        public AssetBucketViewModel? Bucket => _bucket;

        public string DisplayName => _assetDesc.Name.ToString();

        /// <summary>The virtual folder the asset is filed under. If empty, it's at the root</summary>
        public string FolderPath { get; init; } = string.Empty;

        public bool IsPrefab => _bucket?.BucketIndex == AssetBucket.Prefabs.Value;

        public bool IsWorld => _bucket?.BucketIndex == AssetBucket.Worlds.Value;

        public bool IsMesh => _bucket?.BucketIndex == AssetBucket.Meshes.Value;

        public bool HasSceneActions => IsPrefab || IsWorld;

        public bool CanGenerateCollision => IsMesh;

        public string ToolTipText => _bucket != null
            ? $"{DisplayName}\n{_bucket.Name}"
            : DisplayName;

        /// <summary>Asset class name extracted from the manifest (e.g. "MeshAsset"), or null if unavailable.</summary>
        public string? TypeName => _typeName;

        /// <summary>Last-write time of the manifest file on disk, or null if unavailable.</summary>
        public DateTime? DateModified => _dateModified;

        public bool HasManifest => _hasManifest;

        private bool _isEditingName;
        public bool IsEditingName
        {
            get => _isEditingName;
            set => SetProperty(ref _isEditingName, value);
        }

        private string _editingName = string.Empty;
        public string EditingName
        {
            get => _editingName;
            set => SetProperty(ref _editingName, value);
        }

        public void BeginRename()
        {
            EditingName = DisplayName;
            IsEditingName = true;
        }

        public void CancelRename()
        {
            IsEditingName = false;
        }

        // @TODO also, dont just switch on typename -- switch on class -- we dont want to add a new entry for EVERY subclass !
        public string IconKind => _typeName != null
            ? AssetIconHelper.FromTypeName(_typeName)
            : (_bucket != null ? AssetIconHelper.FromBucket(_bucket.Bucket) : "File");

        /// <summary>Reads the type and date from the asset's manifest the first time the asset is listed.</summary>
        public void EnsureFileInfo()
        {
            if (_hasFileInfo || _bucket == null)
            {
                return;
            }

            _hasFileInfo = true;

            string manifestPath = Path.Combine(AssetManager.Instance.AssetRegistry.GetRootPath(), _bucket.Name, DisplayName + ".hmf");

            if (!File.Exists(manifestPath))
            {
                return;
            }

            _hasManifest = true;
            _dateModified = File.GetLastWriteTime(manifestPath);
            _typeName = ReadAssetTypeName(manifestPath);

            OnPropertyChanged(nameof(TypeName));
            OnPropertyChanged(nameof(DateModified));
            OnPropertyChanged(nameof(IconKind));
        }

        /// <summary>
        /// @TODO REMOVE REMOVE REMOVE!!! Find a better way. Go through native code? Bindings for SerializationUtils?
        /// 
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
                return null;
            }

            for (int i = 0; i < count; i++)
            {
                char c = buffer[i];

                if (char.IsWhiteSpace(c) || c == '"' || c == '{')
                {
                    return i > 0 ? new string(buffer.Slice(0, i)) : null;
                }
            }

            return null;
        }

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

        public AssetObjectViewModel(AssetDesc assetDesc, AssetBucketViewModel? bucket = null)
        {
            _assetDesc = assetDesc;
            _bucket = bucket;
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
