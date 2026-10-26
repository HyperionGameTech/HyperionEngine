using System;
using System.Collections.Generic;
using System.Linq;
using System.Reflection;
using System.Windows.Input;
using Avalonia.Controls;
using Avalonia.Controls.Templates;

namespace Hyperion.Editor.Services
{
    /// Marks the class that registers a built-in plugin's editor UI; it's only registered while the plugin is enabled for the open project
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

    public sealed record EditorPluginMenuItem(string Header, ICommand Command);

    /// What one plugin contributed; dropped as a whole when the plugin is disabled
    public sealed class EditorPluginRegistry
    {
        internal readonly Dictionary<Type, Func<Control>> Views = new();
        internal readonly List<EditorPluginMenuItem> ToolMenuItems = new();

        public string PluginName { get; }

        internal EditorPluginRegistry(string pluginName)
        {
            PluginName = pluginName;
        }

        public void RegisterView<TViewModel, TView>() where TView : Control, new()
        {
            Views[typeof(TViewModel)] = () => new TView();
        }

        /// An entry in the toolbar's Tool (gear) menu
        public void AddToolMenuItem(string header, ICommand command)
        {
            ToolMenuItems.Add(new EditorPluginMenuItem(header, command));
        }
    }

    public sealed class EditorPluginService
    {
        public static EditorPluginService Instance { get; } = new EditorPluginService();

        private static readonly LogChannel PluginsLogChannel = LogChannel.ByName("Plugins");

        private readonly Dictionary<string, Type> _pluginTypes = new();
        private readonly Dictionary<string, EditorPluginRegistry> _registries = new();

        public IDataTemplate ViewLocator { get; }

        public IEnumerable<EditorPluginMenuItem> ToolMenuItems => _registries.Values.SelectMany(registry => registry.ToolMenuItems);

        private EditorPluginService()
        {
            ViewLocator = new PluginViewLocator(this);

            foreach (Type type in typeof(EditorPluginService).Assembly.GetTypes())
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

            foreach (string pluginName in _registries.Keys.Where(pluginName => !enabled.Contains(pluginName)).ToList())
            {
                _registries.Remove(pluginName);

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

                Logger.Log(PluginsLogChannel, LogLevel.Info, "Registered editor plugin {0} ({1} view(s), {2} menu item(s))",
                    pluginName, registry.Views.Count, registry.ToolMenuItems.Count);
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
