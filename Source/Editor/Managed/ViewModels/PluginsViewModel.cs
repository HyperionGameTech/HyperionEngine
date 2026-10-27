using System;
using System.Collections.Generic;
using System.Collections.ObjectModel;
using System.Linq;
using System.Threading.Tasks;
using Avalonia.Threading;
using Hyperion;

namespace Hyperion.Editor.ViewModels
{
    public class PluginItemViewModel : ViewModelBase
    {
        private readonly PluginsViewModel _owner;

        public string Name { get; }
        public string Version { get; }
        public string Description { get; }
        public bool AreLibrariesLoaded { get; }
        public IReadOnlyList<string> Dependencies { get; }

        public bool HasDescription => !string.IsNullOrEmpty(Description);

        public PluginItemViewModel(PluginsViewModel owner, string name, string version, string description, bool areLibrariesLoaded, bool isEnabled, IReadOnlyList<string> dependencies)
        {
            _owner = owner;
            Name = name;
            Version = version;
            Description = description;
            AreLibrariesLoaded = areLibrariesLoaded;
            Dependencies = dependencies;
            _isEnabled = isEnabled;
        }

        private bool _isEnabled;
        public bool IsEnabled
        {
            get => _isEnabled;
            set
            {
                if (SetProperty(ref _isEnabled, value))
                {
                    _owner.OnPluginToggled(this);
                }
            }
        }

        internal void SetEnabledQuietly(bool value)
        {
            if (SetProperty(ref _isEnabled, value, nameof(IsEnabled)))
            {
                _owner.OnPluginChanged();
            }
        }
    }

    public class PluginsViewModel : ViewModelBase
    {
        public ObservableCollection<PluginItemViewModel> Plugins { get; } = new();

        private readonly Dictionary<string, bool> _openedWith = new();
        private EditorProject? _project;

        private bool _hasProject;
        public bool HasProject
        {
            get => _hasProject;
            private set => SetProperty(ref _hasProject, value);
        }

        private bool _hasChanges;
        public bool HasChanges
        {
            get => _hasChanges;
            private set => SetProperty(ref _hasChanges, value);
        }

        public PluginsViewModel()
        {
            _ = RefreshAsync();
        }

        private async Task RefreshAsync()
        {
            List<(string Name, string Version, string Description, bool AreLibrariesLoaded, bool IsEnabled, List<string> Dependencies)> rows = new();
            EditorProject? project = null;

            await EngineManager.PostToSimThread(() =>
            {
                project = EngineManager.CurrentProject;

                if (project == null || !project.IsValid)
                {
                    return;
                }

                HashSet<string> enabled = project.GetEnabledPlugins().Cast<Name>().Select(name => name.ToString()).ToHashSet();

                foreach (Name pluginName in project.GetAvailablePlugins().Cast<Name>())
                {
                    rows.Add((
                        pluginName.ToString(),
                        project.GetPluginVersion(pluginName),
                        project.GetPluginDescription(pluginName),
                        project.ArePluginLibrariesLoaded(pluginName),
                        enabled.Contains(pluginName.ToString()),
                        project.GetPluginDependencies(pluginName).Cast<Name>().Select(name => name.ToString()).ToList()));
                }
            });

            await Dispatcher.UIThread.InvokeAsync(() =>
            {
                _project = project;
                HasProject = project != null && project.IsValid;

                Plugins.Clear();
                _openedWith.Clear();

                foreach (var row in rows.OrderBy(row => row.Name, StringComparer.OrdinalIgnoreCase))
                {
                    Plugins.Add(new PluginItemViewModel(this, row.Name, row.Version, row.Description, row.AreLibrariesLoaded, row.IsEnabled, row.Dependencies));
                    _openedWith[row.Name] = row.IsEnabled;
                }

                HasChanges = false;
                NeedsRestart = false;
            });
        }

        internal void OnPluginToggled(PluginItemViewModel plugin)
        {
            if (plugin.IsEnabled)
            {
                foreach (string dependency in plugin.Dependencies)
                {
                    Plugins.FirstOrDefault(item => item.Name == dependency)?.SetEnabledQuietly(true);
                }
            }
            else
            {
                foreach (PluginItemViewModel dependent in Plugins.Where(item => item.Dependencies.Contains(plugin.Name)))
                {
                    dependent.SetEnabledQuietly(false);
                }
            }

            OnPluginChanged();
        }

        internal void OnPluginChanged()
        {
            HasChanges = Plugins.Any(item => _openedWith.TryGetValue(item.Name, out bool wasEnabled) && wasEnabled != item.IsEnabled);
            NeedsRestart = Plugins.Any(item => !item.IsEnabled && _openedWith.TryGetValue(item.Name, out bool wasEnabled) && wasEnabled);
        }

        private bool _needsRestart;
        public bool NeedsRestart
        {
            get => _needsRestart;
            private set => SetProperty(ref _needsRestart, value);
        }

        public async Task ApplyAsync()
        {
            EditorProject? project = _project;

            if (project == null || !HasChanges)
            {
                return;
            }

            List<(string Name, bool IsEnabled)> choices = Plugins.Select(item => (item.Name, item.IsEnabled)).ToList();

            await EngineManager.PostToSimThread(() =>
            {
                if (!project.IsValid)
                {
                    return;
                }

                foreach ((string name, bool isEnabled) in choices)
                {
                    project.SetPluginEnabled(new Name(name), isEnabled);
                }

                project.MarkSettingsDirty();
            });

            EngineManager.EditorGame?.EditorSubsystem?.ExecuteCommandByName(new Name("EditorCommandReopenProject"));
        }
    }
}
