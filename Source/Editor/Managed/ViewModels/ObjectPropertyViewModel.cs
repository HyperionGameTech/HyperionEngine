using System;
using System.Collections.Generic;
using System.Collections.ObjectModel;
using System.ComponentModel;
using System.Diagnostics;
using System.Runtime.InteropServices;
using System.Threading;
using System.Threading.Tasks;
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

        private const long NoSubObjectKey = 0;

        private long _subObjectKey = NoSubObjectKey;

        private static long MakeObjectKey(ObjIdBase id) => ((long)id.TypeId.Value << 32) | id.Value;

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
            private set
            {
                if (SetProperty(ref _hasSubObject, value))
                    OnPropertyChanged(nameof(ShowSubclassPicker));
            }
        }

        private string _iconKind = "File";
        public string IconKind
        {
            get => _iconKind;
            private set => SetProperty(ref _iconKind, value);
        }

        private bool _canCreateNew;
        public bool CanCreateNew
        {
            get => _canCreateNew;
            private set => SetProperty(ref _canCreateNew, value);
        }

        private bool _isEditorExpanded;
        public bool IsEditorExpanded
        {
            get => _isEditorExpanded;
            set => SetProperty(ref _isEditorExpanded, value);
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


        private string _pickerFilter = string.Empty;

        public string PickerFilter
        {
            get => _pickerFilter;
            set => SetProperty(ref _pickerFilter, value);
        }

        private string _currentSelectedName = string.Empty;

        private readonly Class? _propertyTypeClass;

        public ICommand SelectCommand { get; }
        public ICommand ClearCommand { get; }
        public ICommand NewCommand { get; }


        public ObservableCollection<string> AvailableSubclasses { get; } = new();

        public bool IsPolymorphic => AvailableSubclasses.Count > 0;

        public bool ShowSubclassPicker => IsPolymorphic;

        internal bool IsPending { get; set; }
        internal Action<string>? OnPendingCommitted { get; set; }

        private string? _selectedSubclass;
        public string? SelectedSubclass
        {
            get => _selectedSubclass;
            set
            {
                if (SetProperty(ref _selectedSubclass, value) && !string.IsNullOrEmpty(value) && !IsApplyingModelValue)
                {
                    CommitSubclass(value);
                }
            }
        }

        private string _subclassFilter = string.Empty;
        public string SubclassFilter
        {
            get => _subclassFilter;
            set => SetProperty(ref _subclassFilter, value);
        }

        private string _currentTypeName = string.Empty;

        /// <summary>Restore the picker text to the current instance's type name.</summary>
        public void ResetSubclassFilter()
        {
            SubclassFilter = _currentTypeName;
        }

        public ObjectPropertyViewModel(ObjectBase target, Property property, bool isReadOnly, int depth = 0)
            : base(target, property, isReadOnly)
        {
            _depth = depth;

            _isAssetObjectType = DetectIsAssetObjectType(property.TypeInfo);
            _propertyTypeClass = GetPropertyTypeClass(property.TypeInfo);

            SelectCommand = new RelayCommand(OnSelect);
            ClearCommand = new RelayCommand(OnClear);
            NewCommand = new RelayCommand(OnNew);

            HookContentBrowser();
            PopulateSubclasses();
            UpdateCanCreateNew();
        }

        public ObjectPropertyViewModel(IntPtr classAddress, Func<IntPtr> targetAddressResolver, Property property, bool isReadOnly, int depth = 0)
            : base(classAddress, targetAddressResolver, property, isReadOnly)
        {
            _depth = depth;

            _isAssetObjectType = DetectIsAssetObjectType(property.TypeInfo);
            _propertyTypeClass = GetPropertyTypeClass(property.TypeInfo);

            SelectCommand = new RelayCommand(OnSelect);
            ClearCommand = new RelayCommand(OnClear);
            NewCommand = new RelayCommand(OnNew);

            HookContentBrowser();
            PopulateSubclasses();
            UpdateCanCreateNew();
        }

        public ObjectPropertyViewModel(string label, TypeInfo typeInfo, Func<BoxedValue> getter, Action<BoxedValue> setter, bool isReadOnly, int depth = 0)
            : base(label, typeInfo, getter, setter, isReadOnly)
        {
            _depth = depth;

            _isAssetObjectType = DetectIsAssetObjectType(typeInfo);
            _propertyTypeClass = GetPropertyTypeClass(typeInfo);

            SelectCommand = new RelayCommand(OnSelect);
            ClearCommand = new RelayCommand(OnClear);
            NewCommand = new RelayCommand(OnNew);

            HookContentBrowser();
            PopulateSubclasses();
            UpdateCanCreateNew();
        }

        public override bool ShowInlineLabel => false;

        // inline sub-objects list their fields under the label, so the description goes between the two
        public override bool ShowsDescriptionInOwnTemplate => !IsAssetObject;

        private void UpdateCanCreateNew()
        {
            IconKind = AssetIconHelper.FromTypeName(_propertyTypeClass?.Name.ToString());

            if (!_isAssetObjectType || _isReadOnly || _propertyTypeClass == null)
            {
                CanCreateNew = false;
                return;
            }

            Class expected = _propertyTypeClass.Value;

            if (expected.IsAbstract)
            {
                CanCreateNew = false;
                return;
            }

            // Scripts are created through the New Script panel (language + file), not as blank registry entries.
            Class? scriptAssetClass = Class.TryGetClass<ScriptAsset>();

            if (scriptAssetClass.HasValue
                && (expected == scriptAssetClass.Value || expected.IsSubclassOf(scriptAssetClass.Value)))
            {
                CanCreateNew = false;
                return;
            }

            CanCreateNew = true;
        }

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

        private void PopulateSubclasses()
        {
            if (_isAssetObjectType || _propertyTypeClass == null)
                return;

            string className = _propertyTypeClass.Value.Name.ToString();

            List<string> names = [];
            NameCallbackDelegate callback = (name, _) => names.Add(name);

            NativeBindings.Hyp_GetAllDerivedClassNames(className, callback, IntPtr.Zero);

            foreach (string name in names)
                AvailableSubclasses.Add(name);

            if (AvailableSubclasses.Count > 0)
                OnPropertyChanged(nameof(ShowSubclassPicker));
        }

        private void CommitSubclass(string className)
        {
            if (IsPending)
            {
                OnPendingCommitted?.Invoke(className);
                return;
            }

            if (_isReadOnly)
            {
                return;
            }

            _ = EngineManager.PostToSimThread(() =>
            {
                try
                {
                    BoxedValueInternal result;

                    unsafe
                    {
                        if (!Hyp_CreateInstanceOfClass(className, &result))
                        {
                            Logger.Log(LogLevel.Warning, $"Failed to create instance of class '{className}'");
                            return;
                        }
                    }

                    using BoxedValue boxed = BoxedValue.FromBuffer(result);
                    CommitPropertyChange($"Set {Label} type", boxed);
                }
                catch (Exception ex)
                {
                    Logger.Log(LogLevel.Warning, $"Failed to create subclass instance '{className}': {ex.Message}");

                    Dispatcher.UIThread.Post(RefreshValue);
                }
            });
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

            if (selected?.Bucket == null)
            {
                Dispatcher.UIThread.Post(() => CanSelectFromContentBrowser = false);
                return;
            }

            var assetName = selected.AssetDesc.Name;
            var bucketIndex = selected.Bucket.BucketIndex;

            Class? expectedClass = _propertyTypeClass;
            Debug.Assert(expectedClass != null, "Expected class should not be null for asset object properties");

            if (expectedClass == null)
            {
                return;
            }

            Class capturedExpectedClass = expectedClass.Value;

            _ = EngineManager.PostToSimThread(() =>
            {
                try
                {
                    AssetObject? obj = AssetManager.Instance.AssetRegistry.GetAsset(bucketIndex, assetName);
                    bool compatible = false;

                    if (obj != null && obj.IsValid)
                    {
                        Class objClass = obj.Class;

                        compatible = objClass == capturedExpectedClass || objClass.IsSubclassOf(capturedExpectedClass);
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
            if (!CanSelectFromContentBrowser || _isReadOnly)
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
                    CommitPropertyChange($"Set {Label}", boxed);
                }
                catch (Exception ex)
                {
                    Logger.Log(LogLevel.Warning, $"Failed to set asset property '{Label}': {ex.Message}");

                    Dispatcher.UIThread.Post(RefreshValue);
                }
            });
        }

        private void OnClear()
        {
            if (_isReadOnly)
            {
                return;
            }

            _ = EngineManager.PostToSimThread(() =>
            {
                try
                {
                    using BoxedValue boxed = new BoxedValue(null);
                    CommitPropertyChange($"Clear {Label}", boxed);
                }
                catch (Exception ex)
                {
                    Logger.Log(LogLevel.Warning, $"Failed to clear asset property '{Label}': {ex.Message}");

                    Dispatcher.UIThread.Post(RefreshValue);
                }
            });
        }

        private void OnNew()
        {
            if (!CanCreateNew || _isReadOnly || _propertyTypeClass == null)
            {
                return;
            }

            string className = _propertyTypeClass.Value.Name.ToString();

            _ = EngineManager.PostToSimThread(() =>
            {
                try
                {
                    BoxedValueInternal result;

                    unsafe
                    {
                        if (!Hyp_CreateInstanceOfClass(className, &result))
                        {
                            Logger.Log(LogLevel.Warning, $"Failed to create instance of class '{className}'");
                            return;
                        }
                    }

                    using BoxedValue boxed = BoxedValue.FromBuffer(result);

                    if (boxed.GetValue() is AssetObject assetObj && assetObj.IsValid)
                    {
                        try
                        {
                            AssetManager.Instance.AssetRegistry.PutAssetUnique(assetObj);
                        }
                        catch (Exception ex)
                        {
                            Logger.Log(LogLevel.Warning, $"Failed to register new asset '{className}': {ex.Message}");
                            return;
                        }
                    }

                    CommitPropertyChange($"Create {Label}", boxed);
                    Dispatcher.UIThread.Post(() => IsEditorExpanded = true);
                }
                catch (Exception ex)
                {
                    Logger.Log(LogLevel.Warning, $"Failed to create new asset for property '{Label}': {ex.Message}");

                    Dispatcher.UIThread.Post(RefreshValue);
                }
            });
        }

        public string? GetCopyText()
        {
            if (!_isAssetObjectType || !HasSubObject)
            {
                return null;
            }

            return AssetPathDisplay == "(None)" || AssetPathDisplay == "(Unregistered)"
                ? null
                : AssetPathDisplay;
        }

        public void PasteFromText(string? text)
        {
            if (_isReadOnly || string.IsNullOrWhiteSpace(text))
            {
                return;
            }

            string cleaned = text.Trim().Trim('"', '\'');
            int schemeIndex = cleaned.IndexOf("://", StringComparison.Ordinal);

            if (schemeIndex >= 0)
            {
                cleaned = cleaned.Substring(schemeIndex + 3);
            }

            string? bucketName = null;
            string assetName;

            int slashIndex = cleaned.LastIndexOf('/');

            if (slashIndex >= 0)
            {
                bucketName = cleaned.Substring(0, slashIndex);
                assetName = cleaned.Substring(slashIndex + 1);
            }
            else
            {
                assetName = cleaned;
            }

            if (string.IsNullOrEmpty(assetName))
            {
                return;
            }

            Class? expectedClass = _propertyTypeClass;

            if (expectedClass == null)
            {
                return;
            }

            Class capturedExpected = expectedClass.Value;

            _ = EngineManager.PostToSimThread(() =>
            {
                try
                {
                    AssetRegistry registry = AssetManager.Instance.AssetRegistry;
                    AssetObject? obj = null;

                    if (!string.IsNullOrEmpty(bucketName))
                    {
                        foreach (AssetBucket bucket in AssetBucket.AllBuckets)
                        {
                            if (!string.Equals(bucket.Name, bucketName, StringComparison.OrdinalIgnoreCase))
                            {
                                continue;
                            }

                            obj = registry.GetAsset(bucket.Value, new Name(assetName));
                            break;
                        }

                        // Tolerate paths that include extra segments - fall back to a bare-name search.
                        obj ??= FindAssetByName(registry, capturedExpected, assetName);
                    }
                    else
                    {
                        obj = FindAssetByName(registry, capturedExpected, assetName);
                    }

                    if (obj == null || !obj.IsValid)
                    {
                        Logger.Log(LogLevel.Warning, $"Paste: asset '{text}' could not be resolved.");
                        return;
                    }

                    Class objClass = obj.Class;

                    if (objClass != capturedExpected && !objClass.IsSubclassOf(capturedExpected))
                    {
                        Logger.Log(LogLevel.Warning, $"Paste: asset '{text}' is not compatible with property '{Label}'.");
                        return;
                    }

                    using BoxedValue boxed = new BoxedValue(obj);
                    CommitPropertyChange($"Set {Label}", boxed);
                }
                catch (Exception ex)
                {
                    Logger.Log(LogLevel.Warning, $"Failed to paste asset property '{Label}': {ex.Message}");

                    Dispatcher.UIThread.Post(RefreshValue);
                }
            });
        }

        private static AssetObject? FindAssetByName(AssetRegistry registry, Class expectedClass, string assetName)
        {
            var name = new Name(assetName);

            foreach (AssetBucket bucket in AssetBucket.AllBuckets)
            {
                AssetObject? obj = registry.GetAsset(bucket.Value, name);

                if (obj == null || !obj.IsValid)
                {
                    continue;
                }

                Class objClass = obj.Class;

                if (objClass == expectedClass || objClass.IsSubclassOf(expectedClass))
                {
                    return obj;
                }
            }

            return null;
        }

        public async Task<IEnumerable<object>> QueryMatchingAssetsAsync(string? search, int maxResults)
        {
            if (!_isAssetObjectType || _propertyTypeClass == null)
            {
                return Array.Empty<object>();
            }

            Class expectedClass = _propertyTypeClass.Value;
            string filter = search ?? string.Empty;

            List<AssetPickerItemViewModel> results = await EngineManager.PostToSimThread(() =>
            {
                var found = new List<AssetPickerItemViewModel>();

                try
                {
                    AssetRegistry registry = AssetManager.Instance.AssetRegistry;

                    foreach (AssetBucket bucket in AssetBucket.AllBuckets)
                    {
                        foreach (AssetDesc desc in registry.GetBucketAssetDescs(bucket.Value))
                        {
                            try
                            {
                                string nameStr = desc.Name.ToString();

                                if (filter.Length != 0 &&
                                    !nameStr.Contains(filter, StringComparison.OrdinalIgnoreCase))
                                {
                                    continue;
                                }

                                AssetObject? obj = registry.GetAsset(bucket.Value, desc.Name);

                                if (obj == null || !obj.IsValid)
                                {
                                    continue;
                                }

                                Class objClass = obj.Class;

                                if (objClass == expectedClass || objClass.IsSubclassOf(expectedClass))
                                {
                                    found.Add(new AssetPickerItemViewModel(desc.Name, bucket.Value, nameStr, objClass.Name.ToString()));
                                }
                            }
                            catch
                            {
                                // Skip any asset we can't resolve.
                            }

                            if (found.Count >= maxResults)
                            {
                                break;
                            }
                        }

                        if (found.Count >= maxResults)
                        {
                            break;
                        }
                    }
                }
                catch (Exception ex)
                {
                    Logger.Log(LogLevel.Warning, $"Failed to query assets for picker: {ex.Message}");
                }

                found.Sort((a, b) => string.CompareOrdinal(a.DisplayName, b.DisplayName));
                return found;
            });

            return results;
        }

        public void CommitPickerItem(AssetPickerItemViewModel item)
        {
            if (_isReadOnly)
            {
                return;
            }

            uint bucketIndex = item.BucketIndex;
            Name assetName = item.AssetName;

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
                    CommitPropertyChange($"Set {Label}", boxed);
                }
                catch (Exception ex)
                {
                    Logger.Log(LogLevel.Warning, $"Failed to set asset property '{Label}': {ex.Message}");

                    Dispatcher.UIThread.Post(RefreshValue);
                }
            });
        }

        /// <summary>
        /// Restores the picker text to the currently assigned asset's name, e.g. when
        /// the user typed a filter but clicked away without selecting anything.
        /// </summary>
        public void ResetFilterToSelection()
        {
            PickerFilter = _currentSelectedName;
        }

        public override void RefreshValue()
        {
            if (IsPending)
                return;

            if (!BeginRefresh())
            {
                return;
            }

            string? expectedTypeName = _propertyTypeClass?.Name.ToString();

            _ = EngineManager.PostToSimThread(() =>
            {
                long resolvedKey = NoSubObjectKey;
                string assetPathDisplay = "(None)";
                string displayName = "(None)";
                string pickerName = string.Empty;
                string iconKind = AssetIconHelper.FromTypeName(expectedTypeName);
                ComponentSubObjectViewModel? newSubObject = null;

                try
                {
                    using BoxedValue boxed = GetPropertyValue();
                    object? val = boxed.GetValue();

                    if (val is ObjectBase obj && obj.IsValid && _depth < MaxDepth)
                    {
                        resolvedKey = MakeObjectKey(obj.Id);
                        displayName = obj.Class.Name.ToString();
                        iconKind = AssetIconHelper.FromTypeName(displayName);

                        if (obj is AssetObject assetObj)
                        {
                            if (assetObj.IsRegistered())
                            {
                                assetPathDisplay = assetObj.Path.ToString();
                                pickerName = assetObj.Name.ToString();
                            }
                            else
                            {
                                assetPathDisplay = "(Unregistered)";
                            }
                        }

                        // Building the sub-object editor reads the class layout and evaluates
                        // editcondition methods on the object, so it has to happen here rather
                        // than on the UI thread. Only build it when the property points somewhere
                        // new - rebuilding on every refresh would tear down open pop-out panels.
                        if (resolvedKey != Volatile.Read(ref _subObjectKey))
                        {
                            // Editing a field on the sub-object (e.g. a collision shape's bounds) has to run the
                            // owning property's post-write too, or the owner never learns that it changed.
                            newSubObject = new ComponentSubObjectViewModel(
                                Label,
                                obj,
                                _depth + 1,
                                preWriteCallback: null,
                                postWriteCallback: () => PostWriteCallback?.Invoke(),
                                valueChangedCallback: () => ValueChangedCallback?.Invoke());
                        }
                    }
                }
                catch (Exception ex)
                {
                    Logger.Log(LogLevel.Warning, $"Inspector failed to read object property '{Label}': {ex.Message}");

                    EndRefresh();

                    return;
                }

                string capturedDisplayName = displayName;
                string capturedAssetPath = assetPathDisplay;
                string capturedPickerName = pickerName;
                string capturedIconKind = iconKind;
                long capturedResolvedKey = resolvedKey;
                ComponentSubObjectViewModel? capturedSubObject = newSubObject;

                Dispatcher.UIThread.Post(() =>
                {
                    try
                    {
                        ApplyModelValue(() =>
                        {
                            UpdateSubObject(capturedSubObject, capturedResolvedKey);

                            Value = capturedDisplayName;
                            AssetPathDisplay = capturedAssetPath;
                            IconKind = capturedIconKind;

                            if (IsPolymorphic)
                            {
                                _currentTypeName = HasSubObject ? capturedDisplayName : string.Empty;
                                SubclassFilter = _currentTypeName;
                            }

                            if (_isAssetObjectType)
                            {
                                _currentSelectedName = capturedPickerName;
                                PickerFilter = capturedPickerName;
                            }
                        });

                        if (_isAssetObjectType)
                        {
                            OnContentBrowserSelectionChanged();
                        }
                    }
                    finally
                    {
                        EndRefresh();
                    }
                });
            });
        }

        private void UpdateSubObject(ComponentSubObjectViewModel? newSubObject, long resolvedKey)
        {
            if (resolvedKey == NoSubObjectKey)
            {
                Volatile.Write(ref _subObjectKey, NoSubObjectKey);

                SubObject?.Dispose();
                SubObject = null;
                HasSubObject = false;
                return;
            }

            if (newSubObject == null)
            {
                SubObject?.RefreshProperties();
                return;
            }

            Volatile.Write(ref _subObjectKey, resolvedKey);

            SubObject?.Dispose();
            SubObject = newSubObject;
            HasSubObject = true;
        }


        [DllImport("hyperion")]
        private static extern unsafe bool Hyp_CreateInstanceOfClass(string className, BoxedValueInternal* pOutBoxed);
    }
}
