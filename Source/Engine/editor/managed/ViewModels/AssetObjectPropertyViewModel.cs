using System;
using System.ComponentModel;
using System.Threading;
using System.Windows.Input;
using Avalonia.Threading;
using Hyperion;
using Hyperion.Editor.Commands;

namespace Hyperion.Editor.ViewModels
{
    public class AssetObjectPropertyViewModel : InspectorPropertyViewModelBase
    {
        private readonly Class _expectedClass;

        private string _assetDisplayName = "(None)";
        public string AssetDisplayName
        {
            get => _assetDisplayName;
            private set => SetProperty(ref _assetDisplayName, value);
        }

        private bool _canSelectFromContentBrowser;
        public bool CanSelectFromContentBrowser
        {
            get => _canSelectFromContentBrowser;
            private set => SetProperty(ref _canSelectFromContentBrowser, value);
        }

        public ICommand SelectCommand { get; }
        public ICommand ClearCommand { get; }

        public AssetObjectPropertyViewModel(ObjectBase target, Property property, bool isReadOnly)
            : base(target, property, isReadOnly)
        {
            _expectedClass = property.TypeInfo.Class!.Value;
            SelectCommand = new RelayCommand(OnSelect);
            ClearCommand = new RelayCommand(OnClear);
            HookContentBrowser();
        }

        public AssetObjectPropertyViewModel(IntPtr classAddress, Func<IntPtr> targetAddressResolver, Property property, bool isReadOnly)
            : base(classAddress, targetAddressResolver, property, isReadOnly)
        {
            _expectedClass = property.TypeInfo.Class!.Value;
            SelectCommand = new RelayCommand(OnSelect);
            ClearCommand = new RelayCommand(OnClear);
            HookContentBrowser();
        }

        private void HookContentBrowser()
        {
            var cbvm = ContentBrowserViewModel.Instance;

            if (cbvm == null)
            {
                return;
            }

            // Use a WeakReference so the content browser's event does not prevent GC of this VM.
            var weakSelf = new WeakReference<AssetObjectPropertyViewModel>(this);
            PropertyChangedEventHandler? handler = null;

            handler = (sender, e) =>
            {
                if (e.PropertyName != nameof(ContentBrowserViewModel.SelectedAsset))
                {
                    return;
                }

                if (weakSelf.TryGetTarget(out var self))
                {
                    self.OnContentBrowserSelectionChanged();
                }
                else if (sender is ContentBrowserViewModel vm)
                {
                    vm.PropertyChanged -= handler;
                }
            };

            cbvm.PropertyChanged += handler;

            // Evaluate initial state
            OnContentBrowserSelectionChanged();
        }

        private void OnContentBrowserSelectionChanged()
        {
            var selected = ContentBrowserViewModel.Instance?.SelectedAsset;

            if (selected?.Bucket == null)
            {
                Dispatcher.UIThread.Post(() => CanSelectFromContentBrowser = false);
                return;
            }

            var assetName = selected.AssetDesc.Name;
            var bucketIndex = selected.Bucket.BucketIndex;
            var expectedClass = _expectedClass;

            _ = EngineManager.PostToSimThread(() =>
            {
                try
                {
                    AssetObject? obj = AssetManager.Instance.AssetRegistry.GetAsset(bucketIndex, assetName);
                    bool compatible = false;

                    if (obj != null && obj.IsValid)
                    {
                        Class objClass = obj.Class;
                        compatible = objClass == expectedClass || objClass.IsSubclassOf(expectedClass);
                    }

                    Dispatcher.UIThread.Post(() => CanSelectFromContentBrowser = compatible);
                }
                catch
                {
                    Dispatcher.UIThread.Post(() => CanSelectFromContentBrowser = false);
                }
            });
        }

        private void OnSelect()
        {
            if (!CanSelectFromContentBrowser)
            {
                return;
            }

            var selected = ContentBrowserViewModel.Instance?.SelectedAsset;

            if (selected?.Bucket == null)
            {
                return;
            }

            var assetName = selected.AssetDesc.Name;
            var bucketIndex = selected.Bucket.BucketIndex;

            _ = EngineManager.PostToSimThread(() =>
            {
                try
                {
                    AssetObject? obj = AssetManager.Instance.AssetRegistry.GetAsset(bucketIndex, assetName);

                    if (obj == null || !obj.IsValid)
                    {
                        return;
                    }

                    using BoxedValue boxed = new BoxedValue(obj);
                    SetPropertyValue(boxed);

                    string friendly = obj.FriendlyName.ToString();
                    string displayName = string.IsNullOrEmpty(friendly) ? obj.Name.ToString() : friendly;

                    Dispatcher.UIThread.Post(() =>
                    {
                        AssetDisplayName = displayName;
                        Value = displayName;
                    });
                }
                catch (Exception ex)
                {
                    Logger.Log(LogLevel.Warning, $"Failed to set asset property '{_property.Name}': {ex.Message}");
                }
            });
        }

        private void OnClear()
        {
            _ = EngineManager.PostToSimThread(() =>
            {
                try
                {
                    using BoxedValue boxed = new BoxedValue(null);
                    SetPropertyValue(boxed);

                    Dispatcher.UIThread.Post(() =>
                    {
                        AssetDisplayName = "(None)";
                        Value = "(None)";
                    });
                }
                catch (Exception ex)
                {
                    Logger.Log(LogLevel.Warning, $"Failed to clear asset property '{_property.Name}': {ex.Message}");
                }
            });
        }

        public override void RefreshValue()
        {
            if (Interlocked.CompareExchange(ref _isRefreshing, 1, 0) == 1)
            {
                return;
            }

            _ = EngineManager.PostToSimThread(() =>
            {
                try
                {
                    using BoxedValue boxed = GetPropertyValue();
                    object? val = boxed.GetValue();

                    string displayName;

                    if (val is AssetObject obj && obj.IsValid)
                    {
                        string friendly = obj.FriendlyName.ToString();
                        displayName = string.IsNullOrEmpty(friendly) ? obj.Name.ToString() : friendly;
                    }
                    else
                    {
                        displayName = "(None)";
                    }

                    Dispatcher.UIThread.Post(() =>
                    {
                        _isRefreshing = 0;

                        AssetDisplayName = displayName;
                        Value = displayName;
                    });
                }
                catch (Exception ex)
                {
                    _isRefreshing = 0;

                    Logger.Log(LogLevel.Warning, $"Inspector failed to read asset property '{_property.Name}': {ex.Message}");
                }
            });
        }
    }
}
