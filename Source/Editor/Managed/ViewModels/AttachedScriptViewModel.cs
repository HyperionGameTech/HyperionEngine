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
            private set => SetProperty(ref _hasScript, value);
        }

        private bool _canSelectFromContentBrowser;
        public bool CanSelectFromContentBrowser
        {
            get => _canSelectFromContentBrowser;
            private set => SetProperty(ref _canSelectFromContentBrowser, value);
        }

        public ICommand SelectCommand { get; }
        public ICommand ClearCommand { get; }

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

                    if (componentPtr != IntPtr.Zero)
                    {
                        using BoxedValue boxed = capturedProperty.Get(capturedClass.Address, componentPtr);
                        object? val = boxed.GetValue();

                        if (val is ScriptAsset scriptAsset && scriptAsset.IsValid)
                        {
                            hasScript = true;
                            string friendly = scriptAsset.FriendlyName.ToString();
                            displayName = string.IsNullOrEmpty(friendly) ? scriptAsset.Name.ToString() : friendly;

                            if (scriptAsset.IsRegistered())
                            {
                                assetPath = scriptAsset.Path.ToString();
                            }
                            else
                            {
                                assetPath = "(Unregistered)";
                            }
                        }
                    }

                    Dispatcher.UIThread.Post(() =>
                    {
                        capturedSelf._isRefreshing = 0;
                        capturedSelf.ScriptDisplay = displayName;
                        capturedSelf.AssetPathDisplay = assetPath;
                        capturedSelf.HasScript = hasScript;
                    });
                }
                catch (Exception ex)
                {
                    capturedSelf._isRefreshing = 0;

                    Logger.Log(LogLevel.Warning, $"AttachedScript: Failed to refresh: {ex.Message}");
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

                    if (obj == null || !obj.IsValid || obj is not ScriptAsset scriptAsset)
                    {
                        return;
                    }

                    EntityManager? mgr = _entity.EntityManager;

                    if (mgr == null)
                    {
                        return;
                    }

                    // Capture old value for undo
                    ScriptAsset? oldValue = null;
                    IntPtr componentPtr = mgr.GetComponentPtr(_entity, _scriptComponentTypeId);

                    if (componentPtr != IntPtr.Zero)
                    {
                        using BoxedValue oldBoxed = _assetRefProperty.Get(s_scriptComponentClass!.Value.Address, componentPtr);
                        oldValue = oldBoxed.GetValue() as ScriptAsset;
                    }

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
                    using BoxedValue newBoxed = new BoxedValue(scriptAsset);
                    _assetRefProperty.Set(s_scriptComponentClass!.Value.Address, componentPtr, newBoxed);

                    // Push undo action
                    ScriptAsset? capturedOldValue = oldValue;
                    ScriptAsset newValue = scriptAsset;

                    Entity capturedEntity = _entity;
                    Property capturedProperty = _assetRefProperty;
                    Class capturedClass = s_scriptComponentClass.Value;
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

                        using BoxedValue bv = new BoxedValue(valueObj);
                        capturedProperty.Set(capturedClass.Address, ptr, bv);
                    }

                    EditorProject? project = EngineManager.CurrentProject;
                    Debug.Assert(project != null, "No active project found when setting script");

                    project.ActionStack.PushAction(new EditorAction(
                        "Set Script",
                        execute: (_, _) => ApplyValue(newValue),
                        revert: (_, _) => ApplyValue(capturedOldValue)));

                    // Update display
                    string friendly = scriptAsset.FriendlyName.ToString();
                    string displayName = string.IsNullOrEmpty(friendly) ? scriptAsset.Name.ToString() : friendly;
                    string pathDisplay = scriptAsset.IsRegistered() ? scriptAsset.Path.ToString() : "(Unregistered)";

                    Dispatcher.UIThread.Post(() =>
                    {
                        capturedSelf.ScriptDisplay = displayName;
                        capturedSelf.AssetPathDisplay = pathDisplay;
                        capturedSelf.HasScript = true;
                    });
                }
                catch (Exception ex)
                {
                    Logger.Log(LogLevel.Warning, $"AttachedScript: Failed to set script: {ex.Message}");
                }
            });
        }

        private void OnClear()
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

                    // Capture old value for undo
                    object? oldValue = null;

                    if (componentPtr != IntPtr.Zero)
                    {
                        using BoxedValue oldBoxed = _assetRefProperty.Get(s_scriptComponentClass!.Value.Address, componentPtr);
                        oldValue = oldBoxed.GetValue();
                    }

                    if (componentPtr == IntPtr.Zero)
                    {
                        return; // Already clear
                    }

                    // Set to null
                    using BoxedValue nullBoxed = new BoxedValue(null);
                    _assetRefProperty.Set(s_scriptComponentClass!.Value.Address, componentPtr, nullBoxed);

                    // Push undo action
                    object? capturedOldValue = oldValue;

                    Entity capturedEntity = _entity;
                    Property capturedProperty = _assetRefProperty;
                    Class capturedClass = s_scriptComponentClass.Value;
                    TypeId capturedTypeId = _scriptComponentTypeId;
                    AttachedScriptViewModel capturedSelf = this;

                    void ApplyClear(object? valueObj)
                    {
                        EntityManager? m = capturedEntity.EntityManager;

                        if (m == null)
                        {
                            return;
                        }

                        IntPtr ptr = m.GetComponentPtr(capturedEntity, capturedTypeId);

                        if (ptr == IntPtr.Zero)
                        {
                            return;
                        }

                        using BoxedValue bv = new BoxedValue(valueObj);
                        capturedProperty.Set(capturedClass.Address, ptr, bv);
                    }

                    EditorProject? project = EngineManager.CurrentProject;
                    Debug.Assert(project != null, "No active project found when clearing script");

                    project.ActionStack.PushAction(new EditorAction(
                        "Clear Script",
                        execute: (_, _) => ApplyClear(null),
                        revert: (_, _) => ApplyClear(capturedOldValue)));

                    Dispatcher.UIThread.Post(() =>
                    {
                        capturedSelf.ScriptDisplay = "(None)";
                        capturedSelf.AssetPathDisplay = "(None)";
                        capturedSelf.HasScript = false;
                    });
                }
                catch (Exception ex)
                {
                    Logger.Log(LogLevel.Warning, $"AttachedScript: Failed to clear script: {ex.Message}");
                }
            });
        }
    }
}
