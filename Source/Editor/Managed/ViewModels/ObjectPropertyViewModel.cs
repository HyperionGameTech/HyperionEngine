using System;
using System.Collections.Generic;
using System.Collections.ObjectModel;
using System.ComponentModel;
using System.Diagnostics;
using System.Runtime.InteropServices;
using System.Threading;
using System.Threading.Tasks;
using System.Windows.Input;
using Avalonia.Media;
using Avalonia.Threading;
using Hyperion;
using Hyperion.Editor.Commands;
using Hyperion.Editor.Services;

namespace Hyperion.Editor.ViewModels
{
    /// <summary>One implementation offered by an object property's "create new" menu.</summary>
    public class CreatableClassViewModel
    {
        public string ClassName { get; }

        public ICommand CreateCommand { get; }

        public CreatableClassViewModel(string className, Action<string> create)
        {
            ClassName = className;

            CreateCommand = new RelayCommand(() => create(className));
        }
    }

    public class ObjectPropertyViewModel : InspectorPropertyViewModelBase
    {
        private const int MaxDepth = 4;

        private readonly int _depth;
        private readonly bool _isAssetObjectType;

        private const long NoSubObjectKey = 0;

        private long _subObjectKey = NoSubObjectKey;

        private static long MakeObjectKey(ObjIdBase id) => ((long)id.TypeId.Value << 32) | id.Value;

        private static long MakeObjectsKey(List<ObjectBase> objects)
        {
            long key = 17;

            foreach (ObjectBase obj in objects)
            {
                key = unchecked(key * 31 + MakeObjectKey(obj.Id));
            }

            return key == NoSubObjectKey ? 1 : key;
        }

        // Sim thread.
        private void RunAllOwnersPostWrite()
        {
            PostWriteCallback?.Invoke();

            foreach (PropertyTarget peer in Peers)
            {
                peer.PostWrite?.Invoke();
            }
        }

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

                UpdateCanClone();
            }
        }

        private string _iconKind = "File";
        public string IconKind
        {
            get => _iconKind;
            private set => SetProperty(ref _iconKind, value);
        }

        private IImage? _thumbnail;

        /// <summary>The assigned asset's content browser thumbnail, for asset types the thumbnail renderer can draw.</summary>
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

        // the asset the thumbnail subscription is for, so it can be dropped when the property points elsewhere
        private string? _thumbnailAssetKey;
        private uint _thumbnailBucketIndex;
        private Name _thumbnailAssetName;
        private Action<IImage>? _thumbnailCallback;

        private bool _canCreateNew;
        public bool CanCreateNew
        {
            get => _canCreateNew && EngineManager.CanCreateAssets;
            private set => SetProperty(ref _canCreateNew, value);
        }

        /// <summary>Concrete classes the "create new" button can instantiate for this property.</summary>
        public ObservableCollection<CreatableClassViewModel> CreatableClasses { get; } = new();

        /// <summary>
        /// The property's declared type cannot be instantiated on its own, so creating one means choosing
        /// an implementation: the "create new" button opens a menu instead of creating outright.
        /// </summary>
        public bool HasCreatableClassChoice => CreatableClasses.Count > 1;

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
        public ICommand CloneCommand { get; }

        private bool _canClone;
        public bool CanClone
        {
            get => _canClone && EngineManager.CanCreateAssets;
            private set => SetProperty(ref _canClone, value);
        }


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
            CloneCommand = new RelayCommand(OnClone);

            HookContentBrowser();
            PopulateSubclasses();
            UpdateCanCreateNew();
            UpdateCanClone();
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
            CloneCommand = new RelayCommand(OnClone);

            HookContentBrowser();
            PopulateSubclasses();
            UpdateCanCreateNew();
            UpdateCanClone();
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
            CloneCommand = new RelayCommand(OnClone);

            HookContentBrowser();
            PopulateSubclasses();
            UpdateCanCreateNew();
            UpdateCanClone();
        }

        public override bool ShowInlineLabel => false;

        // inline sub-objects list their fields under the label, so the description goes between the two
        public override bool ShowsDescriptionInOwnTemplate => !IsAssetObject;

        private void UpdateCanCreateNew()
        {
            IconKind = AssetIconHelper.FromTypeName(_propertyTypeClass?.Name.ToString());

            PopulateCreatableClasses();

            CanCreateNew = CreatableClasses.Count > 0;

            OnPropertyChanged(nameof(HasCreatableClassChoice));
        }

        private void UpdateCanClone()
        {
            CanClone = _isAssetObjectType && !_isReadOnly && HasSubObject;
        }

        private void PopulateCreatableClasses()
        {
            CreatableClasses.Clear();

            if (!_isAssetObjectType || _isReadOnly || _propertyTypeClass == null)
            {
                return;
            }

            Class expected = _propertyTypeClass.Value;

            // Scripts are created through the New Script panel (language + file), not as blank registry entries.
            Class? scriptAssetClass = Class.TryGetClass<ScriptAsset>();

            if (scriptAssetClass.HasValue
                && (expected == scriptAssetClass.Value || expected.IsSubclassOf(scriptAssetClass.Value)))
            {
                return;
            }

            if (!expected.IsAbstract)
            {
                CreatableClasses.Add(new CreatableClassViewModel(expected.Name.ToString(), CreateNewOfClass));
                return;
            }

            // An abstract property type (PhysicsShape, say) has no instance of its own to create, so the
            // choice of which implementation to create is the user's.
            List<string> derivedNames = [];
            NameCallbackDelegate callback = (name, _) => derivedNames.Add(name);

            NativeBindings.Hyp_GetAllDerivedClassNames(expected.Name.ToString(), callback, IntPtr.Zero);

            derivedNames.Sort(StringComparer.Ordinal);

            foreach (string derivedName in derivedNames)
            {
                CreatableClasses.Add(new CreatableClassViewModel(derivedName, CreateNewOfClass));
            }
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
                // One instance per selected object - they must not end up sharing one. Held until the
                // commit has written them.
                List<BoxedValue> instances = new List<BoxedValue>();

                try
                {
                    for (int i = 0; i < 1 + Peers.Count; i++)
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

                        instances.Add(BoxedValue.FromBuffer(result));
                    }

                    int nextInstance = 0;
                    CommitPropertyChange($"Set {Label} type", _ => instances[nextInstance++].GetValue());
                }
                catch (Exception ex)
                {
                    Logger.Log(LogLevel.Warning, $"Failed to create subclass instance '{className}': {ex.Message}");

                    Dispatcher.UIThread.Post(RefreshValue);
                }
                finally
                {
                    foreach (BoxedValue instance in instances)
                    {
                        instance.Dispose();
                    }
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
            // With more than one implementation to pick from the button hosts a menu instead, and each
            // entry calls CreateNewOfClass directly.
            if (CreatableClasses.Count != 1)
            {
                return;
            }

            CreateNewOfClass(CreatableClasses[0].ClassName);
        }

        private void CreateNewOfClass(string className)
        {
            if (!CanCreateNew || _isReadOnly)
            {
                return;
            }

            _ = EngineManager.PostToSimThread(() =>
            {
                if (!EngineManager.CanCreateAssets)
                {
                    Logger.Log(LogLevel.Warning, $"Cannot create a new asset for property '{Label}' while simulation is active.");

                    return;
                }

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

        /// <summary>Duplicates the currently assigned asset and reassigns this property to the clone.</summary>
        private void OnClone()
        {
            if (!CanClone || _isReadOnly)
            {
                return;
            }

            _ = EngineManager.PostToSimThread(() =>
            {
                if (!EngineManager.CanCreateAssets)
                {
                    Logger.Log(LogLevel.Warning, $"Cannot clone asset for property '{Label}' while simulation is active.");

                    return;
                }

                try
                {
                    using BoxedValue currentBoxed = GetPropertyValue();

                    if (currentBoxed.GetValue() is not AssetObject currentAssetObj || !currentAssetObj.IsValid)
                    {
                        return;
                    }

                    AssetObject? clonedAssetObj = currentAssetObj.InvokeNativeMethod<AssetObject>("CloneAsset");

                    if (clonedAssetObj == null || !clonedAssetObj.IsValid)
                    {
                        Logger.Log(LogLevel.Warning, $"Failed to clone asset for property '{Label}'");
                        return;
                    }

                    try
                    {
                        AssetManager.Instance.AssetRegistry.PutAssetUnique(clonedAssetObj);
                    }
                    catch (Exception ex)
                    {
                        Logger.Log(LogLevel.Warning, $"Failed to register cloned asset '{Label}': {ex.Message}");
                        return;
                    }

                    using BoxedValue boxed = new BoxedValue(clonedAssetObj);
                    CommitPropertyChange($"Clone {Label}", boxed);
                    //Dispatcher.UIThread.Post(() => IsEditorExpanded = true);
                }
                catch (Exception ex)
                {
                    Logger.Log(LogLevel.Warning, $"Failed to clone asset for property '{Label}': {ex.Message}");

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
                bool hasThumbnailAsset = false;
                uint thumbnailBucketIndex = 0;
                Name thumbnailAssetName = default;
                ComponentSubObjectViewModel? newSubObject = null;
                bool isShared;

                try
                {
                    List<object?> values = ReadAllTargets(boxed => boxed.GetValue());
                    object? val = values[0];

                    isShared = values.TrueForAll(v => ValuesEqual(v, val));

                    if (!isShared)
                    {
                        assetPathDisplay = string.Empty;
                        displayName = string.Empty;

                        List<ObjectBase> objects = values.OfType<ObjectBase>().Where(o => o.IsValid).ToList();

                        // Each selected object has its own instance of the same class (e.g. a collision
                        // shape per entity): edit those instances together. Differing assets are left blank.
                        if (!_isAssetObjectType
                            && _depth < MaxDepth
                            && objects.Count == values.Count
                            && objects.TrueForAll(o => o.Class == objects[0].Class))
                        {
                            displayName = objects[0].Class.Name.ToString();
                            iconKind = AssetIconHelper.FromTypeName(displayName);
                            resolvedKey = MakeObjectsKey(objects);

                            if (resolvedKey != Volatile.Read(ref _subObjectKey))
                            {
                                List<(ObjectBase Target, Action? PostWrite)> peerObjects = new List<(ObjectBase, Action?)>();

                                for (int i = 1; i < objects.Count; i++)
                                {
                                    peerObjects.Add((objects[i], Peers[i - 1].PostWrite));
                                }

                                newSubObject = new ComponentSubObjectViewModel(
                                    Label,
                                    objects[0],
                                    _depth + 1,
                                    preWriteCallback: null,
                                    postWriteCallback: () => PostWriteCallback?.Invoke(),
                                    valueChangedCallback: () => ValueChangedCallback?.Invoke(),
                                    peers: peerObjects);
                            }
                        }
                    }
                    else if (val is ObjectBase obj && obj.IsValid && _depth < MaxDepth)
                    {
                        resolvedKey = MakeObjectKey(obj.Id);
                        displayName = obj.Class.Name.ToString();
                        iconKind = AssetIconHelper.FromTypeName(displayName);

                        if (obj is AssetObject assetObj)
                        {
                            if (assetObj.IsRegistered())
                            {
                                AssetPath assetPath = assetObj.Path;

                                assetPathDisplay = assetPath.ToString();
                                pickerName = assetObj.Name.ToString();

                                hasThumbnailAsset = true;
                                thumbnailBucketIndex = assetPath.BucketIndex;
                                thumbnailAssetName = assetObj.Name;
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
                            // owning property's post-write too, or the owner never learns that it changed. With a
                            // multi-selection sharing one object (e.g. the same material), that's every owner.
                            newSubObject = new ComponentSubObjectViewModel(
                                Label,
                                obj,
                                _depth + 1,
                                preWriteCallback: null,
                                postWriteCallback: RunAllOwnersPostWrite,
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

                bool capturedIsShared = isShared;
                string capturedDisplayName = displayName;
                string capturedAssetPath = assetPathDisplay;
                string capturedPickerName = pickerName;
                string capturedIconKind = iconKind;
                long capturedResolvedKey = resolvedKey;
                ComponentSubObjectViewModel? capturedSubObject = newSubObject;
                bool capturedHasThumbnailAsset = hasThumbnailAsset;
                uint capturedThumbnailBucketIndex = thumbnailBucketIndex;
                Name capturedThumbnailAssetName = thumbnailAssetName;

                Dispatcher.UIThread.Post(() =>
                {
                    try
                    {
                        ApplyModelValue(() =>
                        {
                            UpdateSubObject(capturedSubObject, capturedResolvedKey);

                            HasMixedValues = !capturedIsShared;
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
                            UpdateThumbnail(capturedHasThumbnailAsset, capturedThumbnailBucketIndex, capturedThumbnailAssetName);
                        }
                    }
                    finally
                    {
                        EndRefresh();
                    }
                });
            });
        }

        private void UpdateThumbnail(bool hasAsset, uint bucketIndex, Name assetName)
        {
            Dispatcher.UIThread.VerifyAccess();

            string? assetKey = hasAsset ? $"{bucketIndex}/{assetName}" : null;

            if (assetKey == _thumbnailAssetKey)
            {
                return;
            }

            if (_thumbnailCallback != null)
            {
                ThumbnailService.Instance?.Unsubscribe(_thumbnailBucketIndex, _thumbnailAssetName, _thumbnailCallback);
            }

            _thumbnailAssetKey = assetKey;
            _thumbnailCallback = null;
            Thumbnail = null;

            ThumbnailService? thumbnailService = ThumbnailService.Instance;

            if (assetKey == null || thumbnailService == null)
            {
                return;
            }

            _thumbnailBucketIndex = bucketIndex;
            _thumbnailAssetName = assetName;

            Action<IImage>? callback = null;
            callback = image =>
            {
                // an image for an asset this property no longer points at
                if (_thumbnailCallback == callback)
                {
                    Thumbnail = image;
                }
            };

            _thumbnailCallback = callback;

            // asset types the renderer can't draw never call back, and keep showing the type icon
            thumbnailService.Request(bucketIndex, assetName, callback);
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
