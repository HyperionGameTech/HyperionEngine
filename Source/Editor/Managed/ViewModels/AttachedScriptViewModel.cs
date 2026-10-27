using System;
using System.Collections.Generic;
using System.Collections.ObjectModel;
using System.ComponentModel;
using System.Diagnostics;
using System.IO;
using System.Linq;
using System.Runtime.InteropServices;
using System.Threading;
using System.Threading.Tasks;
using System.Windows.Input;
using Avalonia;
using Avalonia.Controls.ApplicationLifetimes;
using Avalonia.Input.Platform;
using Avalonia.Threading;
using Hyperion;
using Hyperion.Editor.Commands;
using Hyperion.Editor.Services;

namespace Hyperion.Editor.ViewModels
{
    public class AttachedScriptViewModel : ViewModelBase
    {
        private readonly Entity _entity;

        private string _scriptDisplay = "(None)";
        public string ScriptDisplay
        {
            get => _scriptDisplay;
            private set => SetProperty(ref _scriptDisplay, value);
        }

        private string _assetPathDisplay = "(None)";
        public string AssetPathDisplay
        {
            get => _assetPathDisplay;
            private set => SetProperty(ref _assetPathDisplay, value);
        }

        private bool _hasScript;
        public bool HasScript
        {
            get => _hasScript;
            private set
            {
                if (SetProperty(ref _hasScript, value))
                {
                    OnPropertyChanged(nameof(CanCopy));
                    OnPropertyChanged(nameof(IconToolTip));
                }
            }
        }

        public bool CanCopy => GetCopyText() != null;

        public string IconToolTip => HasScript ? "Open in code editor" : "Create new script";

        // The script's [HypField] / [HypProperty] members
        public ObservableCollection<InspectorPropertyViewModelBase> Fields { get; } = new();

        private bool _hasFields;
        public bool HasFields
        {
            get => _hasFields;
            private set => SetProperty(ref _hasFields, value);
        }

        private bool _canSelectFromContentBrowser;
        public bool CanSelectFromContentBrowser
        {
            get => _canSelectFromContentBrowser;
            private set => SetProperty(ref _canSelectFromContentBrowser, value);
        }

        public ICommand SelectCommand { get; }
        public ICommand ClearCommand { get; }
        public ICommand NewCommand { get; }
        public ICommand EditCommand { get; }
        public ICommand IconCommand { get; }
        public ICommand CopyCommand { get; }
        public ICommand PasteCommand { get; }

        private static readonly Class? s_scriptComponentClass = Class.TryGetClass("ScriptComponent");
        private static readonly Class? s_scriptAssetClass = Class.TryGetClass<ScriptAsset>();

        private readonly Property _assetRefProperty = Property.Invalid;
        private readonly TypeId _scriptComponentTypeId;
        private int _isRefreshing;

        public AttachedScriptViewModel(Entity entity)
        {
            _entity = entity;
            SelectCommand = new RelayCommand(OnSelect);
            ClearCommand = new RelayCommand(OnClear);
            NewCommand = new RelayCommand(OnNew, () => EngineManager.CanCreateAssets);
            EditCommand = new RelayCommand(OnEdit);
            IconCommand = new RelayCommand(OnIcon);
            CopyCommand = new AsyncRelayCommand(OnCopyAsync);
            PasteCommand = new AsyncRelayCommand(OnPasteAsync);

            Debug.Assert(s_scriptComponentClass.HasValue);

            if (s_scriptComponentClass.HasValue)
            {
                Class cls = s_scriptComponentClass.Value;
                _scriptComponentTypeId = cls.TypeId;
                _assetRefProperty = cls.GetProperty(new Name("Script")) ?? Property.Invalid;
            }
            else
            {
                _assetRefProperty = Property.Invalid;
            }

            HookContentBrowser();
            Refresh();
        }

        private void HookContentBrowser()
        {
            var cbvm = ContentBrowserViewModel.Instance;

            if (cbvm == null)
            {
                return;
            }

            var weakSelf = new WeakReference<AttachedScriptViewModel>(this);
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

            _ = EngineManager.PostToSimThread(() =>
            {
                try
                {
                    AssetObject? obj = AssetManager.Instance.AssetRegistry.GetAsset(bucketIndex, assetName);
                    bool compatible = false;

                    if (obj != null && obj.IsValid && s_scriptAssetClass.HasValue)
                    {
                        Class objClass = obj.Class;

                        compatible = objClass == s_scriptAssetClass.Value || objClass.IsSubclassOf(s_scriptAssetClass.Value);
                    }

                    Dispatcher.UIThread.Post(() => CanSelectFromContentBrowser = compatible);
                }
                catch
                {
                    Dispatcher.UIThread.Post(() => CanSelectFromContentBrowser = false);
                }
            });
        }

        public void Refresh()
        {
            if (Interlocked.CompareExchange(ref _isRefreshing, 1, 0) == 1)
            {
                return;
            }

            if (_assetRefProperty == Property.Invalid || !s_scriptComponentClass.HasValue)
            {
                _isRefreshing = 0;
                return;
            }

            var capturedEntity = _entity;
            var capturedProperty = _assetRefProperty;
            var capturedClass = s_scriptComponentClass.Value;
            var capturedTypeId = _scriptComponentTypeId;
            var capturedSelf = this;

            _ = EngineManager.PostToSimThread(() =>
            {
                try
                {
                    EntityManager? mgr = capturedEntity.EntityManager;

                    if (mgr == null)
                    {
                        capturedSelf._isRefreshing = 0;
                        return;
                    }

                    IntPtr componentPtr = mgr.GetComponentPtr(capturedEntity, capturedTypeId);

                    string displayName = "(None)";
                    string assetPath = "(None)";
                    bool hasScript = false;
                    List<InspectorPropertyViewModelBase> fields = new();

                    if (componentPtr != IntPtr.Zero)
                    {
                        using BoxedValue boxed = capturedProperty.Get(capturedClass.Address, componentPtr);
                        object? val = boxed.GetValue();

                        if (val is ScriptAsset scriptAsset && scriptAsset.IsValid)
                        {
                            hasScript = true;
                            displayName = scriptAsset.Name.ToString() ;

                            if (scriptAsset.IsRegistered())
                            {
                                assetPath = scriptAsset.Path.ToString();
                            }
                            else
                            {
                                assetPath = "(Unregistered)";
                            }

                            fields = capturedSelf.CreateFieldViewModels(componentPtr);
                        }
                    }

                    Dispatcher.UIThread.Post(() =>
                    {
                        capturedSelf._isRefreshing = 0;
                        capturedSelf.ScriptDisplay = displayName;
                        capturedSelf.AssetPathDisplay = assetPath;
                        capturedSelf.HasScript = hasScript;

                        capturedSelf.Fields.Clear();

                        foreach (InspectorPropertyViewModelBase field in fields)
                        {
                            capturedSelf.Fields.Add(field);
                        }

                        capturedSelf.HasFields = capturedSelf.Fields.Count > 0;
                    });
                }
                catch (Exception ex)
                {
                    capturedSelf._isRefreshing = 0;

                    Logger.Log(LogLevel.Warning, $"AttachedScript: Failed to refresh: {ex.Message}");
                }
            });
        }

        // Sim thread. Values are read from the live script object; edits also go into the ScriptComponent so they're saved
        private List<InspectorPropertyViewModelBase> CreateFieldViewModels(IntPtr componentPtr)
        {
            List<InspectorPropertyViewModelBase> vms = new();

            IntPtr scriptClassPtr = ScriptComponent_GetScriptClass(componentPtr);

            if (scriptClassPtr == IntPtr.Zero)
            {
                return vms;
            }

            Class scriptClass = new(scriptClassPtr);
            Entity capturedEntity = _entity;
            TypeId scriptComponentTypeId = _scriptComponentTypeId;

            IntPtr ResolveComponentPtr()
            {
                EntityManager? mgr = capturedEntity.EntityManager;

                return mgr != null ? mgr.GetComponentPtr(capturedEntity, scriptComponentTypeId) : IntPtr.Zero;
            }

            IEnumerable<Property> fieldProperties = scriptClass.Properties
                .Where(p =>
                {
                    ClassAttribute? attrEditor = p.GetAttribute("editor");

                    return attrEditor == null || attrEditor.Value.GetBool();
                })
                .OrderBy(p =>
                {
                    ClassAttribute? attrEditorOrder = p.GetAttribute("editororder");

                    return attrEditorOrder != null ? attrEditorOrder.Value.GetInt() : int.MaxValue;
                })
                .ThenBy(p => p.Name.ToString());

            foreach (Property property in fieldProperties)
            {
                try
                {
                    ClassAttribute? attrLabel = property.GetAttribute("label");
                    ClassAttribute? attrEditEnabled = property.GetAttribute("editenabled");

                    string label = attrLabel != null ? attrLabel.Value.GetString() : property.Name.ToString();
                    bool isReadOnly = attrEditEnabled != null && attrEditEnabled.Value.GetBool() == false;

                    Name propertyName = property.Name;

                    // the script's class is replaced when its assembly reloads, so it's looked up again on every access
                    Property? ResolveProperty(IntPtr ptr, out IntPtr classAddress)
                    {
                        classAddress = ptr != IntPtr.Zero ? ScriptComponent_GetScriptClass(ptr) : IntPtr.Zero;

                        return classAddress != IntPtr.Zero ? new Class(classAddress).GetProperty(propertyName) : null;
                    }

                    InspectorPropertyViewModelBase vm = InspectorViewModelFactory.CreateForValue(
                        label,
                        property.TypeInfo,
                        getter: () =>
                        {
                            IntPtr ptr = ResolveComponentPtr();
                            Property? current = ResolveProperty(ptr, out IntPtr classAddress);
                            IntPtr scriptObjectPtr = ScriptComponent_GetScriptObject(ptr);

                            return current.HasValue && scriptObjectPtr != IntPtr.Zero
                                ? current.Value.Get(classAddress, scriptObjectPtr)
                                : new BoxedValue(null);
                        },
                        setter: value =>
                        {
                            IntPtr ptr = ResolveComponentPtr();
                            Property? current = ResolveProperty(ptr, out _);

                            if (current.HasValue)
                            {
                                ScriptComponent_SetFieldValue(ptr, current.Value.Address, ref value.Buffer);
                            }
                        },
                        isReadOnly: isReadOnly,
                        initialize: false,
                        postWriteCallback: () =>
                        {
                            IntPtr ptr = ResolveComponentPtr();
                            Property? current = ResolveProperty(ptr, out _);

                            if (current.HasValue)
                            {
                                ScriptComponent_StoreFieldValue(ptr, current.Value.Address);
                            }

                            capturedEntity.MarkDirty();
                        },
                        declaredTypeResolver: () =>
                        {
                            Property? current = ResolveProperty(ResolveComponentPtr(), out IntPtr classAddress);

                            return current.HasValue ? DeclaredObjectClass.FindMemberType(new Class(classAddress), propertyName) : null;
                        });

                    vm.RefreshValue();

                    vms.Add(vm);
                }
                catch (Exception ex)
                {
                    Logger.Log(LogLevel.Debug, $"Inspector skipping script field '{property.Name}': {ex.Message}");
                }
            }

            return vms;
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

                    if (obj == null || !obj.IsValid || obj is not ScriptAsset scriptAsset)
                    {
                        return;
                    }

                    AssignScriptAssetOnSimThread(scriptAsset);
                }
                catch (Exception ex)
                {
                    Logger.Log(LogLevel.Warning, $"AttachedScript: Failed to set script: {ex.Message}");
                }
            });
        }

        private void OnNew()
        {
            var panel = new NewScriptPanelViewModel((name, languageArg) =>
            {
                if (string.IsNullOrEmpty(name) || string.IsNullOrEmpty(languageArg))
                {
                    Logger.Log(LogLevel.Warning, "New script creation cancelled.");
                    return;
                }

                _ = EngineManager.PostToSimThread(() =>
                {
                    if (!EngineManager.CanCreateAssets)
                    {
                        Logger.Log(LogLevel.Warning, "Cannot create a new script while simulation is active.");

                        return;
                    }

                    try
                    {
                        if (AssetManager.Instance.AssetRegistry.GetUniqueAssetName(AssetBucket.Scripts.Value, name).ToString() != name)
                        {
                            Logger.Log(LogLevel.Warning, $"AttachedScript: A script named '{name}' already exists.");
                            return;
                        }

                        EngineManager.EditorGame?.EditorSubsystem?.ExecuteCommandByName(
                            new Name("EditorCommandNewScript"), $"{languageArg} {name}");

                        ScriptAsset? scriptAsset = AssetManager.Instance.AssetRegistry.GetAsset(
                            AssetBucket.Scripts.Value, new Name(name)) as ScriptAsset;

                        if (scriptAsset != null && scriptAsset.IsValid)
                        {
                            AssignScriptAssetOnSimThread(scriptAsset);
                        }
                    }
                    catch (Exception ex)
                    {
                        Logger.Log(LogLevel.Warning, $"AttachedScript: Failed to create script: {ex.Message}");
                    }
                });
            });

            PanelService.Instance.OpenPanel(panel);
        }

        private void OnEdit()
        {
            _ = EngineManager.PostToSimThread(() =>
            {
                try
                {
                    EntityManager? mgr = _entity.EntityManager;

                    if (mgr == null)
                    {
                        return;
                    }

                    IntPtr componentPtr = mgr.GetComponentPtr(_entity, _scriptComponentTypeId);

                    if (componentPtr == IntPtr.Zero)
                    {
                        return;
                    }

                    using BoxedValue boxed = _assetRefProperty.Get(s_scriptComponentClass!.Value.Address, componentPtr);

                    if (boxed.GetValue() is not ScriptAsset scriptAsset || !scriptAsset.IsValid)
                    {
                        return;
                    }

                    ScriptDesc scriptDesc = scriptAsset.ScriptDesc;
                    string scriptPath = Path.Combine(AssetManager.Instance.AssetRegistry.GetRootPath(), scriptDesc.Path);

                    Dispatcher.UIThread.Post(() => CodeEditorService.OpenFile(scriptPath));
                }
                catch (Exception ex)
                {
                    Logger.Log(LogLevel.Warning, $"AttachedScript: Failed to open script: {ex.Message}");
                }
            });
        }

        private void OnIcon()
        {
            if (HasScript)
            {
                OnEdit();
            }
            else if (EngineManager.CanCreateAssets)
            {
                OnNew();
            }
        }

        private void AssignScriptAsset(ScriptAsset? scriptAsset)
        {
            _ = EngineManager.PostToSimThread(() => AssignScriptAssetOnSimThread(scriptAsset));
        }

        private void AssignScriptAssetOnSimThread(ScriptAsset? scriptAsset)
        {
            try
            {
                EntityManager? mgr = _entity.EntityManager;

                if (mgr == null)
                {
                    return;
                }

                IntPtr componentPtr = mgr.GetComponentPtr(_entity, _scriptComponentTypeId);

                // Capture old value for undo
                ScriptAsset? oldValue = null;

                if (componentPtr != IntPtr.Zero)
                {
                    using BoxedValue oldBoxed = _assetRefProperty.Get(s_scriptComponentClass!.Value.Address, componentPtr);
                    oldValue = oldBoxed.GetValue() as ScriptAsset;
                }

                if (scriptAsset != null)
                {
                    // Add component if it doesn't exist
                    if (componentPtr == IntPtr.Zero)
                    {
                        mgr.AddDefaultComponent(_entity, s_scriptComponentClass!.Value);
                        componentPtr = mgr.GetComponentPtr(_entity, _scriptComponentTypeId);
                    }

                    if (componentPtr == IntPtr.Zero)
                    {
                        Logger.Log(LogLevel.Warning, "AttachedScript: Failed to get or create ScriptComponent");
                        return;
                    }

                    // Set the script asset
                    using BoxedValue newBoxed = new(scriptAsset);
                    _assetRefProperty.Set(s_scriptComponentClass!.Value.Address, componentPtr, newBoxed);
                }
                else
                {
                    if (componentPtr == IntPtr.Zero)
                    {
                        return; // Already clear
                    }

                    // Set to null
                    using BoxedValue nullBoxed = new(null);
                    _assetRefProperty.Set(s_scriptComponentClass!.Value.Address, componentPtr, nullBoxed);
                }

                ScriptSystem_RecreateEntityScript(_entity.NativeAddress);

                // Push undo action
                ScriptAsset? capturedOldValue = oldValue;
                ScriptAsset? capturedNewValue = scriptAsset;

                Entity capturedEntity = _entity;
                Property capturedProperty = _assetRefProperty;
                Class capturedClass = s_scriptComponentClass!.Value;
                TypeId capturedTypeId = _scriptComponentTypeId;
                AttachedScriptViewModel capturedSelf = this;

                void ApplyValue(ScriptAsset? valueObj)
                {
                    EntityManager? m = capturedEntity.EntityManager;

                    if (m == null)
                    {
                        return;
                    }

                    IntPtr ptr = m.GetComponentPtr(capturedEntity, capturedTypeId);

                    if (ptr == IntPtr.Zero)
                    {
                        if (valueObj == null)
                        {
                            return; // already cleared, nothing to do
                        }

                        m.AddDefaultComponent(capturedEntity, capturedClass);
                        ptr = m.GetComponentPtr(capturedEntity, capturedTypeId);
                    }

                    if (ptr == IntPtr.Zero)
                    {
                        return;
                    }

                    using BoxedValue bv = new(valueObj);
                    capturedProperty.Set(capturedClass.Address, ptr, bv);

                    ScriptSystem_RecreateEntityScript(capturedEntity.NativeAddress);

                    capturedSelf.Refresh();
                }

                EditorProject? project = EngineManager.CurrentProject;
                Debug.Assert(project != null, "No active project found when setting script");

                (EngineManager.GetActionStackFor(capturedEntity) ?? project.ActionStack).PushAction(new EditorAction(
                    scriptAsset != null ? "Set Script" : "Clear Script",
                    execute: (_, _) => ApplyValue(capturedNewValue),
                    revert: (_, _) => ApplyValue(capturedOldValue)));

                Refresh();
            }
            catch (Exception ex)
            {
                Logger.Log(LogLevel.Warning, $"AttachedScript: Failed to set script: {ex.Message}");
            }
        }

        private void OnClear()
        {
            AssignScriptAsset(null);
        }

        public string? GetCopyText()
        {
            if (!HasScript)
            {
                return null;
            }

            return AssetPathDisplay == "(None)" || AssetPathDisplay == "(Unregistered)"
                ? null
                : AssetPathDisplay;
        }

        private static IClipboard? GetClipboard()
            => (Application.Current?.ApplicationLifetime as IClassicDesktopStyleApplicationLifetime)?.MainWindow?.Clipboard;

        private async Task OnCopyAsync(object? dontCare = null)
        {
            if (GetCopyText() is not string text)
            {
                return;
            }

            IClipboard? clipboard = GetClipboard();

            if (clipboard != null)
            {
                await clipboard.SetTextAsync(text);
            }
        }

        private async Task OnPasteAsync(object? dontCare = null)
        {
            IClipboard? clipboard = GetClipboard();

            if (clipboard == null)
            {
                return;
            }

            string? text = await clipboard.TryGetTextAsync();
            PasteFromText(text);
        }

        public void PasteFromText(string? text)
        {
            if (string.IsNullOrWhiteSpace(text))
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

            string? capturedBucketName = bucketName;

            _ = EngineManager.PostToSimThread(() =>
            {
                try
                {
                    AssetRegistry registry = AssetManager.Instance.AssetRegistry;
                    ScriptAsset? scriptAsset = null;

                    if (!string.IsNullOrEmpty(capturedBucketName))
                    {
                        foreach (AssetBucket bucket in AssetBucket.AllBuckets)
                        {
                            if (!string.Equals(bucket.Name, capturedBucketName, StringComparison.OrdinalIgnoreCase))
                            {
                                continue;
                            }

                            scriptAsset = registry.GetAsset(bucket.Value, new Name(assetName)) as ScriptAsset;

                            if (scriptAsset != null && !scriptAsset.IsValid)
                            {
                                scriptAsset = null;
                            }

                            break;
                        }

                        // Tolerate paths that include extra segments - fall back to a bare-name search.
                        scriptAsset ??= FindScriptAssetByName(registry, assetName);
                    }
                    else
                    {
                        scriptAsset = FindScriptAssetByName(registry, assetName);
                    }

                    if (scriptAsset == null || !scriptAsset.IsValid)
                    {
                        Logger.Log(LogLevel.Warning, $"Paste: script asset '{text}' could not be resolved.");
                        return;
                    }

                    AssignScriptAssetOnSimThread(scriptAsset);
                }
                catch (Exception ex)
                {
                    Logger.Log(LogLevel.Warning, $"AttachedScript: Failed to paste script: {ex.Message}");
                }
            });
        }

        private static ScriptAsset? FindScriptAssetByName(AssetRegistry registry, string assetName)
        {
            var name = new Name(assetName);

            foreach (AssetBucket bucket in AssetBucket.AllBuckets)
            {
                if (registry.GetAsset(bucket.Value, name) is ScriptAsset found && found.IsValid)
                {
                    return found;
                }
            }

            return null;
        }


        ////////////////////

        [DllImport("hyperion", EntryPoint = "ScriptComponent_GetScriptClass")]
        private static extern IntPtr ScriptComponent_GetScriptClass(IntPtr scriptComponent);

        [DllImport("hyperion", EntryPoint = "ScriptComponent_GetScriptObject")]
        private static extern IntPtr ScriptComponent_GetScriptObject(IntPtr scriptComponent);

        [DllImport("hyperion", EntryPoint = "ScriptComponent_SetFieldValue")]
        [return: MarshalAs(UnmanagedType.I1)]
        private static extern bool ScriptComponent_SetFieldValue(IntPtr scriptComponent, IntPtr property, [In] ref BoxedValueInternal value);

        [DllImport("hyperion", EntryPoint = "ScriptComponent_StoreFieldValue")]
        [return: MarshalAs(UnmanagedType.I1)]
        private static extern bool ScriptComponent_StoreFieldValue(IntPtr scriptComponent, IntPtr property);

        [DllImport("hyperion", EntryPoint = "ScriptSystem_RecreateEntityScript")]
        private static extern void ScriptSystem_RecreateEntityScript(IntPtr entity);
    }
}
