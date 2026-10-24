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
    public class ObjectPropertyViewModel : InspectorPropertyViewModelBase
    {
        private const int MaxDepth = 8;

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
                    OnPropertyChanged(nameof(SubclassDisplay));

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
        private readonly Func<Type?>? _declaredTypeResolver;

        public ICommand SelectCommand { get; }
        public ICommand ClearCommand { get; }
        public ICommand NewCommand { get; }
        public ICommand CloneCommand { get; }
        public ICommand ImportCommand { get; }

        public bool CanImport => _isAssetObjectType && !_isReadOnly;

        private bool _canClone;
        public bool CanClone
        {
            get => _canClone && EngineManager.CanCreateAssets;
            private set => SetProperty(ref _canClone, value);
        }


        private TypeId? _propertyTypeId;

        public bool IsPolymorphic => _propertyTypeId.HasValue;

        public bool ShowSubclassPicker => IsPolymorphic;

        internal bool IsPending { get; set; }
        internal Action<string>? OnPendingCommitted { get; set; }

        private string _currentTypeName = string.Empty;
        public string CurrentTypeName
        {
            get => _currentTypeName;
            private set
            {
                if (SetProperty(ref _currentTypeName, value))
                    OnPropertyChanged(nameof(SubclassDisplay));
            }
        }

        public string SubclassDisplay => HasSubObject ? _currentTypeName : HasMixedValues ? "—" : "None";

        public void PickSubclass(string? className)
        {
            if (className == null)
            {
                if (!IsPending && (HasSubObject || HasMixedValues))
                    OnClear();

                return;
            }

            if (!HasMixedValues && HasSubObject && className == _currentTypeName)
                return;

            CommitSubclass(className);
        }

        public ObjectPropertyViewModel(ObjectBase target, Property property, bool isReadOnly, int depth = 0)
            : base(target, property, isReadOnly)
        {
            _depth = depth;

            _declaredTypeResolver = () => DeclaredObjectClass.FindMemberType(target.Class, property.Name);
            _propertyTypeClass = DeclaredObjectClass.Resolve(GetPropertyTypeClass(property.TypeInfo), _declaredTypeResolver());
            _isAssetObjectType = IsAssetObjectClass(_propertyTypeClass);

            SelectCommand = new RelayCommand(OnSelect);
            ClearCommand = new RelayCommand(OnClear);
            NewCommand = new RelayCommand(OnNew);
            CloneCommand = new RelayCommand(OnClone);
            ImportCommand = new RelayCommand(OnImport);

            HookContentBrowser();
            PopulateSubclasses();
            UpdateCanCreateNew();
            UpdateCanClone();
        }

        public ObjectPropertyViewModel(IntPtr classAddress, Func<IntPtr> targetAddressResolver, Property property, bool isReadOnly, int depth = 0)
            : base(classAddress, targetAddressResolver, property, isReadOnly)
        {
            _depth = depth;

            _declaredTypeResolver = () => DeclaredObjectClass.FindMemberType(new Class(classAddress), property.Name);
            _propertyTypeClass = DeclaredObjectClass.Resolve(GetPropertyTypeClass(property.TypeInfo), _declaredTypeResolver());
            _isAssetObjectType = IsAssetObjectClass(_propertyTypeClass);

            SelectCommand = new RelayCommand(OnSelect);
            ClearCommand = new RelayCommand(OnClear);
            NewCommand = new RelayCommand(OnNew);
            CloneCommand = new RelayCommand(OnClone);
            ImportCommand = new RelayCommand(OnImport);

            HookContentBrowser();
            PopulateSubclasses();
            UpdateCanCreateNew();
            UpdateCanClone();
        }

        public ObjectPropertyViewModel(string label, TypeInfo typeInfo, Func<BoxedValue> getter, Action<BoxedValue> setter, bool isReadOnly, int depth = 0, Func<Type?>? declaredTypeResolver = null)
            : base(label, typeInfo, getter, setter, isReadOnly)
        {
            _depth = depth;

            _declaredTypeResolver = declaredTypeResolver;
            _propertyTypeClass = DeclaredObjectClass.Resolve(GetPropertyTypeClass(typeInfo), _declaredTypeResolver?.Invoke());
            _isAssetObjectType = IsAssetObjectClass(_propertyTypeClass);

            SelectCommand = new RelayCommand(OnSelect);
            ClearCommand = new RelayCommand(OnClear);
            NewCommand = new RelayCommand(OnNew);
            CloneCommand = new RelayCommand(OnClone);
            ImportCommand = new RelayCommand(OnImport);

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

            CanCreateNew = BuildCreatableClassEntries().Any(e => !e.IsAbstract);
        }

        private void UpdateCanClone()
        {
            CanClone = _isAssetObjectType && !_isReadOnly && HasSubObject;
        }

        public IReadOnlyList<ClassPickerEntry> BuildCreatableClassEntries()
        {
            if (!_isAssetObjectType || _isReadOnly || _propertyTypeClass == null)
            {
                return [];
            }

            Class expected = _propertyTypeClass.Value;

            // Scripts are created through the New Script panel (language + file), not as blank registry entries.
            Class? scriptAssetClass = Class.TryGetClass<ScriptAsset>();

            if (scriptAssetClass.HasValue
                && (expected == scriptAssetClass.Value || expected.IsSubclassOf(scriptAssetClass.Value)))
            {
                return [];
            }

            return ClassPickerEntries.BuildCreatableEntries(expected);
        }

        private static bool IsAssetObjectClass(Class? propertyClass)
        {
            if (!propertyClass.HasValue)
            {
                return false;
            }

            Class? assetClass = Class.TryGetClass<AssetObject>();

            if (!assetClass.HasValue)
            {
                return false;
            }

            return propertyClass.Value == assetClass.Value || propertyClass.Value.IsSubclassOf(assetClass.Value);
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
            _propertyTypeId = _isAssetObjectType ? null : _propertyTypeClass?.TypeId;
        }

        public IReadOnlyList<ClassPickerEntry> BuildSubclassEntries()
        {
            return _propertyTypeId.HasValue ? ClassPickerEntries.BuildDerived(_propertyTypeId.Value) : [];
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
                List<BoxedValue> instances = new();

                try
                {
                    Type? declaredType = _declaredTypeResolver?.Invoke();

                    for (int i = 0; i < 1 + Peers.Count; i++)
                    {
                        if (DeclaredObjectClass.CreateManagedInstance(declaredType, className) is object managedInstance)
                        {
                            instances.Add(new BoxedValue(managedInstance));
                            continue;
                        }

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
                    CommitPropertyChange($"Set {Label} type", current =>
                    {
                        object? instance = instances[nextInstance++].GetValue();

                        if (current.GetValue() is ObjectBase previous
                            && previous.IsValid
                            && instance is ObjectBase created
                            && created.IsValid)
                        {
                            CopyMatchingProperties(previous, created);
                        }

                        return instance;
                    });
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

        // Sim thread. Inline sub-objects are left behind rather than shared between the old and new instance.
        private static void CopyMatchingProperties(ObjectBase source, ObjectBase destination)
        {
            Class sourceClass = source.Class;

            foreach (Property property in destination.Class.Properties)
            {
                ClassAttribute? attrEditor = property.GetAttribute("editor");

                if (attrEditor != null && attrEditor.Value.GetBool() == false)
                {
                    continue;
                }

                Property? sourceProperty = sourceClass.GetProperty(property.Name);

                if (sourceProperty == null || !sourceProperty.Value.TypeId.Equals(property.TypeId))
                {
                    continue;
                }

                try
                {
                    using BoxedValue value = sourceProperty.Value.Get(source);

                    if (value.GetValue() is ObjectBase inner && inner is not AssetObject)
                    {
                        continue;
                    }

                    property.Set(destination, value);
                }
                catch (Exception ex)
                {
                    Logger.Log(LogLevel.Warning, $"Failed to carry '{property.Name}' over to the new '{destination.Class.Name}': {ex.Message}");
                }
            }
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

                    using BoxedValue boxed = new(obj);
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
                    using BoxedValue boxed = new(null);
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
            if (!CanCreateNew || _isReadOnly)
            {
                return;
            }

            string? folderPath = ContentBrowserViewModel.Instance?.CurrentFolder?.Path;

            _ = EngineManager.PostToSimThread(() =>
            {
                IReadOnlyList<ClassPickerEntry> entries = BuildCreatableClassEntries();

                if (!entries.Any(e => !e.IsAbstract))
                {
                    return;
                }

                Dispatcher.UIThread.Post(() =>
                {
                    PanelService.Instance.OpenPanel(new NewAssetPanelViewModel(entries, CreatePendingAsset, asset =>
                    {
                        if (asset != null)
                        {
                            _ = EngineManager.PostToSimThread(() => CommitNewAsset(asset, folderPath));
                        }
                    }));
                });
            });
        }

        // Sim thread.
        private AssetObject? CreatePendingAsset(string className)
        {
            if (DeclaredObjectClass.CreateManagedInstance(_declaredTypeResolver?.Invoke(), className) is AssetObject managedAsset)
            {
                return managedAsset;
            }

            BoxedValueInternal result;

            unsafe
            {
                if (!Hyp_CreateInstanceOfClass(className, &result))
                {
                    Logger.Log(LogLevel.Warning, $"Failed to create instance of class '{className}'");
                    return null;
                }
            }

            using BoxedValue boxed = BoxedValue.FromBuffer(result);

            return boxed.GetValue() as AssetObject;
        }

        // Sim thread.
        private void CommitNewAsset(AssetObject assetObj, string? folderPath)
        {
            if (!EngineManager.CanCreateAssets)
            {
                Logger.Log(LogLevel.Warning, $"Cannot create a new asset for property '{Label}' while simulation is active.");

                return;
            }

            try
            {
                AssetManager.Instance.AssetRegistry.PutAssetUnique(assetObj);

                if (!assetObj.IsRegistered())
                {
                    Logger.Log(LogLevel.Warning, $"New '{assetObj.Class.Name}' for '{Label}' could not be added to the asset registry");
                    return;
                }

                if (folderPath != null)
                {
                    EngineManager.CurrentProject?.AssetFolders?.SetAssetFolderOf(assetObj.Path.BucketIndex, assetObj.Name, folderPath);
                }

                ContentBrowserViewModel.Instance?.NotifyAssetsChanged();

                using BoxedValue boxed = new(assetObj);
                CommitPropertyChange($"Create {Label}", boxed);
            }
            catch (Exception ex)
            {
                Logger.Log(LogLevel.Warning, $"Failed to create new asset for property '{Label}': {ex.Message}");

                Dispatcher.UIThread.Post(RefreshValue);
            }
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

                    using BoxedValue boxed = new(clonedAssetObj);
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

        /// <summary>
        /// Imports a file picked from disk into the asset registry and assigns the result to this property.
        /// Shows an error message box if the import fails or the imported asset doesn't fit the property type.
        /// </summary>
        private void OnImport()
        {
            if (!CanImport)
            {
                return;
            }

            Class? expectedClass = _propertyTypeClass;

            if (expectedClass == null)
            {
                return;
            }

            Class capturedExpectedClass = expectedClass.Value;

            _ = EngineManager.PostToSimThread(() =>
            {
                AssetImportService.ImportWithDialog(result =>
                {
                    if (result.Cancelled)
                    {
                        return;
                    }

                    if (!result.Succeeded)
                    {
                        ShowImportError(result.Error);
                        return;
                    }

                    _ = EngineManager.PostToSimThread(() => AssignImportedAsset(result, capturedExpectedClass));
                });
            });
        }

        private void AssignImportedAsset(AssetImportResult result, Class expectedClass)
        {
            try
            {
                AssetObject? obj = AssetManager.Instance.AssetRegistry.GetAsset(result.BucketIndex, result.AssetName);

                if (obj == null || !obj.IsValid)
                {
                    ShowImportError($"Imported asset '{result.AssetName}' could not be found in the asset registry.");
                    return;
                }

                Class objClass = obj.Class;

                if (obj is Prefab importedPrefab && expectedClass.Name == nameof(Mesh))
                {
                    Mesh? largestMesh = importedPrefab.GetLargestMesh();

                    if (largestMesh == null || !largestMesh.IsValid)
                    {
                        ShowImportError($"Imported prefab asset '{result.AssetName}' contains no meshes that can be assigned to '{Label}'.");
                        return;
                    }

                    uint numMeshes = importedPrefab.CountMeshes();

                    if (numMeshes > 1)
                    {
                        string warning = $"Imported prefab asset '{result.AssetName}' contains {numMeshes} meshes.\n"
                            + "The largest mesh by world-space bounds has been selected ('{largestMesh.Name}'), as only one Mesh may be selected for '{Label}'.";

                        Logger.Log(LogLevel.Warning, $"Import: {warning}");

                        Dispatcher.UIThread.Post(() => MessageBox.Warning("Multiple Meshes Found", warning).Show());
                    }

                    obj = largestMesh;
                    objClass = obj.Class;
                }

                if (objClass != expectedClass && !objClass.IsSubclassOf(expectedClass))
                {
                    ShowImportError($"Imported asset '{result.AssetName}' is a {objClass.Name}, which can't be assigned to '{Label}' (expects {expectedClass.Name}).");
                    return;
                }

                using BoxedValue boxed = new(obj);
                CommitPropertyChange($"Import {Label}", boxed);
            }
            catch (Exception ex)
            {
                ShowImportError($"Failed to assign imported asset to '{Label}': {ex.Message}");

                Dispatcher.UIThread.Post(RefreshValue);
            }
        }

        private static void ShowImportError(string message)
        {
            Logger.Log(LogLevel.Warning, $"Import: {message}");

            Dispatcher.UIThread.Post(() => MessageBox.Critical("Import Failed", message).Show());
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

                    using BoxedValue boxed = new(obj);
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
                string? firstAssetError = null;

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
                            catch (Exception ex)
                            {
                                // Skip any asset we can't resolve.
                                firstAssetError ??= $"'{desc.Name}': {ex.Message}";
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

                if (firstAssetError != null)
                {
                    Logger.Log(LogLevel.Warning, $"Asset picker for '{Label}' skipped assets it could not resolve (first: {firstAssetError})");
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

                    using BoxedValue boxed = new(obj);
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
                                pickerName = $"{displayName} (unregistered)";
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
                                CurrentTypeName = HasSubObject ? capturedDisplayName : string.Empty;
                                OnPropertyChanged(nameof(SubclassDisplay));
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
