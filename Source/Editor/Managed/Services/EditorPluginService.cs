using System;
using System.Collections.Generic;
using System.Collections.ObjectModel;
using System.Linq;
using System.Reflection;
using System.Windows.Input;
using Avalonia.Controls;
using Avalonia.Controls.Templates;
using Hyperion.Editor.ViewModels;

namespace Hyperion.Editor.Services
{
    [AttributeUsage(AttributeTargets.Class)]
    public sealed class EditorPluginAttribute : Attribute
    {
        public string PluginName { get; }

        public EditorPluginAttribute(string pluginName)
        {
            PluginName = pluginName;
        }
    }

    public interface IEditorPlugin
    {
        void Register(EditorPluginRegistry registry);
    }

    public sealed record EditorPluginMenuItem(string Header, ICommand Command, Func<bool>? IsChecked = null);

    /// An entry in a scene hierarchy node's context menu; canExecute runs on the sim thread when the menu opens
    public sealed record EditorNodeMenuItem(string Header, Func<Node, bool> CanExecute, Action<Node> Execute);

    public delegate InspectorPropertyViewModelBase? PropertyEditorFactory(IntPtr classAddress, Func<IntPtr> targetAddressResolver, Property property, bool isReadOnly, Entity ownerEntity, bool isNameType);

    /// A dock tab in the documents area, shown while IsOpen; its content comes from the view registered for its type
    public abstract class EditorDocumentViewModel : ViewModelBase
    {
        public abstract string Id { get; }

        private string _title = string.Empty;
        public string Title
        {
            get => _title;
            protected set => SetProperty(ref _title, value);
        }

        private bool _isOpen;
        public bool IsOpen
        {
            get => _isOpen;
            protected set => SetProperty(ref _isOpen, value);
        }
    }

    /// A bar under the viewport, shown while IsVisible (usually while its edit mode is active)
    public abstract class EditModeBarViewModel : ViewModelBase
    {
        private bool _isVisible;
        public bool IsVisible
        {
            get => _isVisible;
            protected set => SetProperty(ref _isVisible, value);
        }
    }

    /// What one plugin contributed; dropped as a whole when the plugin is disabled
    public sealed class EditorPluginRegistry
    {
        internal readonly Dictionary<Type, Func<Control>> Views = new();
        internal readonly List<EditorPluginMenuItem> ToolMenuItems = new();
        internal readonly List<EditorNodeMenuItem> NodeMenuItems = new();
        internal readonly List<EditorDocumentViewModel> Documents = new();
        internal readonly List<EditModeBarViewModel> EditModeBars = new();
        internal readonly List<Action<EditorIsolatedEditState?>> EditModeListeners = new();
        internal readonly List<Action<IReadOnlyList<Node>>> SelectionListeners = new();
        internal readonly List<Action<bool>> SimulationListeners = new();
        internal readonly Dictionary<string, PropertyEditorFactory> PropertyEditors = new();
        internal readonly List<(string ComponentClassName, Func<Entity, InspectorPropertyViewModelBase> Create)> ComponentInspectorItems = new();
        internal readonly List<Func<Entity, string?>> InspectorWarnings = new();
        internal readonly Dictionary<string, string> AssetIcons = new();
        internal readonly HashSet<string> EntityDropAssetClasses = new();
        internal readonly HashSet<string> CreatableAssetClasses = new();

        public string PluginName { get; }

        internal EditorPluginRegistry(string pluginName)
        {
            PluginName = pluginName;
        }

        public void RegisterView<TViewModel, TView>() where TView : Control, new()
        {
            Views[typeof(TViewModel)] = () => new TView();
        }

        /// An entry in the toolbar's Tool (gear) menu; with isChecked it shows as a checkbox
        public void AddToolMenuItem(string header, ICommand command, Func<bool>? isChecked = null)
        {
            ToolMenuItems.Add(new EditorPluginMenuItem(header, command, isChecked));
        }

        public void AddNodeMenuItem(string header, Func<Node, bool> canExecute, Action<Node> execute)
        {
            NodeMenuItems.Add(new EditorNodeMenuItem(header, canExecute, execute));
        }

        public void AddDocument(EditorDocumentViewModel document)
        {
            Documents.Add(document);
        }

        public void AddEditModeBar(EditModeBarViewModel bar)
        {
            EditModeBars.Add(bar);
        }

        /// Called on the sim thread when an edit mode is entered, left or changes state, with the active one (or null)
        public void OnEditModeChanged(Action<EditorIsolatedEditState?> listener)
        {
            EditModeListeners.Add(listener);
        }

        /// Called on the UI thread with the scene hierarchy selection
        public void OnSelectionChanged(Action<IReadOnlyList<Node>> listener)
        {
            SelectionListeners.Add(listener);
        }

        /// Called on the UI thread when play starts (true) or stops (false)
        public void OnSimulationChanged(Action<bool> listener)
        {
            SimulationListeners.Add(listener);
        }

        /// The inspector editor for entity component properties with this editortype attribute
        public void RegisterPropertyEditor(string editorType, PropertyEditorFactory factory)
        {
            PropertyEditors[editorType] = factory;
        }

        /// An item shown above the properties of a component of this class
        public void AddComponentInspectorItem(string componentClassName, Func<Entity, InspectorPropertyViewModelBase> create)
        {
            ComponentInspectorItems.Add((componentClassName, create));
        }

        /// A check run on each selected entity; a non-null result is shown as a warning in the inspector
        public void AddInspectorWarning(Func<Entity, string?> check)
        {
            InspectorWarnings.Add(check);
        }

        public void RegisterAssetIcon(string assetClassName, string iconKey)
        {
            AssetIcons[assetClassName] = iconKey;
        }

        /// An asset class that can be dropped onto an entity (applied by a native entity drop handler)
        public void AddEntityDropAssetClass(string assetClassName)
        {
            EntityDropAssetClasses.Add(assetClassName);
        }

        /// An asset class the content browser can create
        public void AddCreatableAssetClass(string assetClassName)
        {
            CreatableAssetClasses.Add(assetClassName);
        }
    }

    public sealed class EditorPluginService
    {
        public static EditorPluginService Instance { get; } = new EditorPluginService();

        private static readonly LogChannel PluginsLogChannel = LogChannel.ByName("Plugins");

        private readonly Dictionary<string, Type> _pluginTypes = new();
        private readonly Dictionary<string, EditorPluginRegistry> _registries = new();

        public IDataTemplate ViewLocator { get; }

        public ObservableCollection<EditorDocumentViewModel> Documents { get; } = new();
        public ObservableCollection<EditModeBarViewModel> EditModeBars { get; } = new();

        public IEnumerable<EditorPluginMenuItem> ToolMenuItems => _registries.Values.SelectMany(registry => registry.ToolMenuItems);
        public IEnumerable<EditorNodeMenuItem> NodeMenuItems => _registries.Values.SelectMany(registry => registry.NodeMenuItems);
        public IEnumerable<Func<Entity, string?>> InspectorWarnings => _registries.Values.SelectMany(registry => registry.InspectorWarnings);

        private readonly HashSet<string> _enabledPlugins = new();

        private EditorPluginService()
        {
            ViewLocator = new PluginViewLocator(this);

            AddPluginTypes(typeof(EditorPluginService).Assembly);
        }

        /// A plugin's editor assembly (Hyperion.Plugin.<Module>), loaded when a project enabled the plugin; its [EditorPlugin] registers if the plugin is enabled
        public void AddPluginAssembly(Assembly assembly)
        {
            Avalonia.Threading.Dispatcher.UIThread.Post(() =>
            {
                AddPluginTypes(assembly);
                SetEnabledPlugins(_enabledPlugins.ToList());
            });
        }

        private void AddPluginTypes(Assembly assembly)
        {
            foreach (Type type in assembly.GetTypes())
            {
                if (type.GetCustomAttribute<EditorPluginAttribute>() is { } attribute && typeof(IEditorPlugin).IsAssignableFrom(type))
                {
                    _pluginTypes[attribute.PluginName] = type;
                }
            }
        }

        /// Registers the editor UI of the plugins enabled for the open project, and drops the rest. Call on the UI thread
        public void SetEnabledPlugins(IEnumerable<string> pluginNames)
        {
            HashSet<string> enabled = pluginNames.ToHashSet();

            _enabledPlugins.Clear();
            _enabledPlugins.UnionWith(enabled);

            foreach (string pluginName in _registries.Keys.Where(pluginName => !enabled.Contains(pluginName)).ToList())
            {
                EditorPluginRegistry registry = _registries[pluginName];
                _registries.Remove(pluginName);

                foreach (EditorDocumentViewModel document in registry.Documents)
                {
                    Documents.Remove(document);
                }

                foreach (EditModeBarViewModel bar in registry.EditModeBars)
                {
                    EditModeBars.Remove(bar);
                }

                Logger.Log(PluginsLogChannel, LogLevel.Info, "Unregistered editor plugin {0}", pluginName);
            }

            foreach (string pluginName in enabled)
            {
                if (_registries.ContainsKey(pluginName) || !_pluginTypes.TryGetValue(pluginName, out Type? pluginType))
                {
                    continue;
                }

                EditorPluginRegistry registry = new EditorPluginRegistry(pluginName);

                try
                {
                    ((IEditorPlugin)Activator.CreateInstance(pluginType)!).Register(registry);
                }
                catch (Exception e)
                {
                    Logger.Log(PluginsLogChannel, LogLevel.Error, "Editor plugin {0} failed to register: {1}", pluginName, e);

                    continue;
                }

                _registries[pluginName] = registry;

                foreach (EditorDocumentViewModel document in registry.Documents)
                {
                    Documents.Add(document);
                }

                foreach (EditModeBarViewModel bar in registry.EditModeBars)
                {
                    EditModeBars.Add(bar);
                }

                Logger.Log(PluginsLogChannel, LogLevel.Info, "Registered editor plugin {0} ({1} view(s), {2} menu item(s))",
                    pluginName, registry.Views.Count, registry.ToolMenuItems.Count + registry.NodeMenuItems.Count);
            }
        }

        public void NotifyEditModeChanged(EditorIsolatedEditState? activeState)
        {
            foreach (EditorPluginRegistry registry in _registries.Values.ToList())
            {
                foreach (Action<EditorIsolatedEditState?> listener in registry.EditModeListeners)
                {
                    Invoke(registry, () => listener(activeState));
                }
            }
        }

        public void NotifySelectionChanged(IReadOnlyList<Node> selectedNodes)
        {
            foreach (EditorPluginRegistry registry in _registries.Values.ToList())
            {
                foreach (Action<IReadOnlyList<Node>> listener in registry.SelectionListeners)
                {
                    Invoke(registry, () => listener(selectedNodes));
                }
            }
        }

        public void NotifySimulationChanged(bool isSimulating)
        {
            foreach (EditorPluginRegistry registry in _registries.Values.ToList())
            {
                foreach (Action<bool> listener in registry.SimulationListeners)
                {
                    Invoke(registry, () => listener(isSimulating));
                }
            }
        }

        public PropertyEditorFactory? FindPropertyEditor(string editorType)
        {
            foreach (EditorPluginRegistry registry in _registries.Values)
            {
                if (registry.PropertyEditors.TryGetValue(editorType, out PropertyEditorFactory? factory))
                {
                    return factory;
                }
            }

            return null;
        }

        public IEnumerable<Func<Entity, InspectorPropertyViewModelBase>> GetComponentInspectorItems(string componentClassName)
        {
            return _registries.Values
                .SelectMany(registry => registry.ComponentInspectorItems)
                .Where(item => item.ComponentClassName == componentClassName)
                .Select(item => item.Create);
        }

        public string? FindAssetIcon(string assetClassName)
        {
            foreach (EditorPluginRegistry registry in _registries.Values)
            {
                if (registry.AssetIcons.TryGetValue(assetClassName, out string? iconKey))
                {
                    return iconKey;
                }
            }

            return null;
        }

        public bool IsEntityDropAssetClass(string assetClassName)
        {
            return _registries.Values.Any(registry => registry.EntityDropAssetClasses.Contains(assetClassName));
        }

        public bool IsCreatableAssetClass(string assetClassName)
        {
            return _registries.Values.Any(registry => registry.CreatableAssetClasses.Contains(assetClassName));
        }

        private static void Invoke(EditorPluginRegistry registry, Action action)
        {
            try
            {
                action();
            }
            catch (Exception e)
            {
                Logger.Log(PluginsLogChannel, LogLevel.Error, "Editor plugin {0} failed handling an editor event: {1}", registry.PluginName, e);
            }
        }

        private Func<Control>? FindView(Type viewModelType)
        {
            foreach (EditorPluginRegistry registry in _registries.Values)
            {
                if (registry.Views.TryGetValue(viewModelType, out Func<Control>? createView))
                {
                    return createView;
                }
            }

            return null;
        }

        private sealed class PluginViewLocator : IDataTemplate
        {
            private readonly EditorPluginService _service;

            public PluginViewLocator(EditorPluginService service)
            {
                _service = service;
            }

            public Control? Build(object? data)
            {
                return data != null ? _service.FindView(data.GetType())?.Invoke() : null;
            }

            public bool Match(object? data)
            {
                return data != null && _service.FindView(data.GetType()) != null;
            }
        }
    }
}
