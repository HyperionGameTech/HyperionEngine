using System;
using System.ComponentModel;
using System.Diagnostics;
using System.Threading;
using System.Windows.Input;
using Avalonia.Threading;
using Hyperion;
using Hyperion.Editor.Commands;

namespace Hyperion.Editor.ViewModels
{
    public class ObjectPropertyViewModel : InspectorPropertyViewModelBase
    {
        private const int MaxDepth = 4;

        private readonly int _depth;
        private readonly bool _isAssetObjectType;

        // Sub-object expansion
        private ComponentSubObjectViewModel? _subObject;
        public ComponentSubObjectViewModel? SubObject
        {
            get => _subObject;
            private set => SetProperty(ref _subObject, value);
        }

        private bool _hasSubObject;
        public bool HasSubObject
        {
            get => _hasSubObject;
            private set => SetProperty(ref _hasSubObject, value);
        }

        public bool IsAssetObject => _isAssetObjectType;

        private string _assetPathDisplay = "(None)";
        public string AssetPathDisplay
        {
            get => _assetPathDisplay;
            private set => SetProperty(ref _assetPathDisplay, value);
        }

        private bool _canSelectFromContentBrowser;
        public bool CanSelectFromContentBrowser
        {
            get => _canSelectFromContentBrowser;
            private set => SetProperty(ref _canSelectFromContentBrowser, value);
        }

        private readonly Class? _propertyTypeClass;

        public ICommand SelectCommand { get; }
        public ICommand ClearCommand { get; }

        public ObjectPropertyViewModel(ObjectBase target, Property property, bool isReadOnly, int depth = 0)
            : base(target, property, isReadOnly)
        {
            _depth = depth;
            _isAssetObjectType = DetectIsAssetObjectType(property.TypeInfo);
            _propertyTypeClass = GetPropertyTypeClass(property.TypeInfo);
            SelectCommand = new RelayCommand(OnSelect);
            ClearCommand = new RelayCommand(OnClear);
            HookContentBrowser();
        }

        public ObjectPropertyViewModel(IntPtr classAddress, Func<IntPtr> targetAddressResolver, Property property, bool isReadOnly, int depth = 0)
            : base(classAddress, targetAddressResolver, property, isReadOnly)
        {
            _depth = depth;
            _isAssetObjectType = DetectIsAssetObjectType(property.TypeInfo);
            _propertyTypeClass = GetPropertyTypeClass(property.TypeInfo);
            SelectCommand = new RelayCommand(OnSelect);
            ClearCommand = new RelayCommand(OnClear);
            HookContentBrowser();
        }

        public override bool ShowInlineLabel => false;

        private static bool DetectIsAssetObjectType(TypeInfo typeInfo)
        {
            if (!typeInfo.IsClass || !typeInfo.Class.HasValue)
            {
                return false;
            }

            Class? assetClass = Class.TryGetClass<AssetObject>();

            if (!assetClass.HasValue)
            {
                return false;
            }

            Class propertyClass = typeInfo.Class.Value;
            return propertyClass == assetClass.Value || propertyClass.IsSubclassOf(assetClass.Value);
        }

        private static Class? GetPropertyTypeClass(TypeInfo typeInfo)
        {
            if (!typeInfo.IsClass || !typeInfo.Class.HasValue)
            {
                return null;
            }

            return typeInfo.Class.Value;
        }

        private void HookContentBrowser()
        {
            if (!_isAssetObjectType)
            {
                return;
            }

            var cbvm = ContentBrowserViewModel.Instance;

            if (cbvm == null)
            {
                return;
            }

            var weakSelf = new WeakReference<ObjectPropertyViewModel>(this);
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

            OnContentBrowserSelectionChanged();
        }

        private void OnContentBrowserSelectionChanged()
        {
            var selected = ContentBrowserViewModel.Instance?.SelectedAsset;

            if (selected?.Package == null)
            {
                Dispatcher.UIThread.Post(() => CanSelectFromContentBrowser = false);
                return;
            }

            var assetName = selected.AssetDesc.Name;
            var package = selected.Package.Package;

            Class? expectedClass = _propertyTypeClass;
            Debug.Assert(expectedClass != null, "Expected class should not be null for asset object properties");

            _ = EngineManager.PostToSimThread(() =>
            {
                try
                {
                    AssetObject? obj = package.GetAssetObject(assetName);
                    bool compatible = false;

                    if (obj != null && obj.IsValid)
                    {
                        Class objClass = obj.Class;

                        compatible = objClass == expectedClass.Value || objClass.IsSubclassOf(expectedClass.Value);
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

            if (selected?.Package == null)
            {
                return;
            }

            var assetName = selected.AssetDesc.Name;
            var package = selected.Package.Package;

            _ = EngineManager.PostToSimThread(() =>
            {
                try
                {
                    AssetObject? obj = package.GetAssetObject(assetName);

                    if (obj == null || !obj.IsValid)
                    {
                        return;
                    }

                    using BoxedValue boxed = new BoxedValue(obj);
                    CommitPropertyChange($"Set {_property.Name}", boxed);
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
                    CommitPropertyChange($"Clear {_property.Name}", boxed);
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

                    ComponentSubObjectViewModel? subObjectVm = null;
                    string assetPathDisplay = "(None)";
                    string displayName = "(None)";

                    if (val is ObjectBase obj && obj.IsValid && _depth < MaxDepth)
                    {
                        subObjectVm = new ComponentSubObjectViewModel(_property.Name.ToString(), obj, _depth + 1, PostWriteCallback);
                        displayName = obj.Class.Name.ToString();

                        if (obj is AssetObject assetObj)
                        {
                            if (assetObj.IsRegistered())
                            {
                                assetPathDisplay = assetObj.Path.ToString();
                            }
                            else
                            {
                                assetPathDisplay = "(Unregistered)";
                            }
                        }
                    }

                    Dispatcher.UIThread.Post(() =>
                    {
                        _isRefreshing = 0;

                        Value = displayName;
                        AssetPathDisplay = assetPathDisplay;
                        SubObject = subObjectVm;
                        HasSubObject = subObjectVm != null;

                        if (_isAssetObjectType)
                        {
                            OnContentBrowserSelectionChanged();
                        }
                    });
                }
                catch (Exception ex)
                {
                    _isRefreshing = 0;

                    Logger.Log(LogLevel.Warning, $"Inspector failed to read object property '{_property.Name}': {ex.Message}");
                }
            });
        }
    }
}
