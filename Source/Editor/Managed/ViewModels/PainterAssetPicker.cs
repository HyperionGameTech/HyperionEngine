using System;
using System.Collections.Generic;
using System.Collections.ObjectModel;
using System.Linq;
using Avalonia.Threading;
using Hyperion;

namespace Hyperion.Editor.ViewModels
{
    public sealed class PainterAssetPicker : IDisposable
    {
        private readonly uint _assetBucketIndex;

        private DelegateHandler? _onAssetsChangedHandler;

        private readonly InspectorPropertyViewModelBase? _activeAssetProperty;

        public ObservableCollection<InspectorPropertyViewModelBase> Properties { get; } = new ObservableCollection<InspectorPropertyViewModelBase>();

        public event Action? AssetBucketChanged;

        public PainterAssetPicker(
            string toolTitle,
            ObjectBase toolState,
            string activeAssetPropertyName,
            AssetBucket assetBucket,
            EditorSubsystem editorSubsystem,
            Action? valueChangedCallback = null,
            params AssetBucket[] relatedBuckets)
        {
            _assetBucketIndex = assetBucket.Value;

            uint[] relatedBucketIndices = relatedBuckets.Select(bucket => bucket.Value).ToArray();

            Property? activeAssetProperty = toolState.Class.GetProperty(new Name(activeAssetPropertyName));

            if (activeAssetProperty != null)
            {
                _activeAssetProperty = InspectorViewModelFactory.Create(toolState, activeAssetProperty.Value, isReadOnly: false, valueChangedCallback: valueChangedCallback);

                Properties.Add(_activeAssetProperty);
            }
            else
            {
                Logger.Log(LogLevel.Error, $"{toolTitle}: tool state has no {activeAssetPropertyName} property");
            }

            // sim thread
            _onAssetsChangedHandler = editorSubsystem.GetOnAssetsChangedDelegate().Bind((uint bucketIndex) =>
            {
                if (bucketIndex != _assetBucketIndex && !relatedBucketIndices.Contains(bucketIndex))
                {
                    return;
                }

                AssetBucketChanged?.Invoke();

                Refresh();
            });
        }

        public void Refresh()
        {
            Dispatcher.UIThread.Post(() => _activeAssetProperty?.RefreshValue());
        }

        public HashSet<string> GetAssetNames()
        {
            AssetRegistry registry = AssetManager.Instance.AssetRegistry;

            return new HashSet<string>(
                registry.GetBucketAssetDescs(_assetBucketIndex).Select(assetDesc => assetDesc.Name.ToString()),
                StringComparer.Ordinal);
        }

        public void Dispose()
        {
            _onAssetsChangedHandler?.Remove();
            _onAssetsChangedHandler?.Dispose();
            _onAssetsChangedHandler = null;
        }
    }
}
