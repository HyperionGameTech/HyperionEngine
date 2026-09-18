using System;
using System.Collections.Generic;
using System.Collections.ObjectModel;
using System.Linq;
using System.Threading.Tasks;
using System.Windows.Input;
using Avalonia.Threading;
using Hyperion;
using Hyperion.Editor.Commands;

namespace Hyperion.Editor.ViewModels
{
    /// <summary>
    /// View model for the World Settings window. Unlike most editor panels this is not a
    /// PanelService-managed dynamic panel - it backs a standalone modal window (similar in
    /// spirit to VS Code's settings editor) and currently exposes management of the active
    /// world's WorldGrid layers (terrain etc.).
    /// </summary>
    public class WorldSettingsPanelViewModel : ViewModelBase
    {
        private const int AutoRefreshIntervalMs = 1500;

        public ObservableCollection<WorldGridLayerViewModel> Layers { get; } = new ObservableCollection<WorldGridLayerViewModel>();

        public ObservableCollection<WorldGridLayerViewModel> FilteredLayers { get; } = new ObservableCollection<WorldGridLayerViewModel>();

        public ObservableCollection<WorldGridLayerTypeViewModel> AvailableLayerTypes { get; } = new ObservableCollection<WorldGridLayerTypeViewModel>();

        private string? _searchText;
        public string? SearchText
        {
            get => _searchText;
            set
            {
                if (SetProperty(ref _searchText, value))
                {
                    ApplyFilter();
                }
            }
        }

        private bool _hasWorldGrid;
        public bool HasWorldGrid
        {
            get => _hasWorldGrid;
            private set
            {
                if (SetProperty(ref _hasWorldGrid, value))
                {
                    OnPropertyChanged(nameof(ShowNoLayersHint));
                }
            }
        }

        private SkySettingsViewModel? _sky;
        public SkySettingsViewModel? Sky
        {
            get => _sky;
            private set
            {
                if (SetProperty(ref _sky, value))
                {
                    OnPropertyChanged(nameof(HasSky));
                }
            }
        }

        public bool HasSky => Sky != null;

        private EnvironmentSettingsViewModel? _environment;
        public EnvironmentSettingsViewModel? Environment
        {
            get => _environment;
            private set
            {
                if (SetProperty(ref _environment, value))
                {
                    OnPropertyChanged(nameof(HasEnvironment));
                }
            }
        }

        public bool HasEnvironment => Environment != null;

        public bool HasLayers => FilteredLayers.Count > 0;

        public bool ShowNoLayersHint => HasWorldGrid && !HasLayers;

        private bool _isRefreshing;
        public bool IsRefreshing
        {
            get => _isRefreshing;
            private set => SetProperty(ref _isRefreshing, value);
        }

        public ICommand AddLayerTypeCommand { get; }
        public ICommand RemoveLayerCommand { get; }

        private readonly DispatcherTimer _autoRefreshTimer;

        public WorldSettingsPanelViewModel()
        {
            AddLayerTypeCommand = new RelayCommand<string>(typeName => _ = AddLayerAsync(typeName));
            RemoveLayerCommand = new RelayCommand<WorldGridLayerViewModel>(layer => _ = RemoveLayerAsync(layer));

            // Keep the layer list in sync with the engine without a manual refresh button.
            // The tick only rebuilds the list when the engine-side layer set actually changed,
            // so in-progress property edits and expander state are not disturbed.
            _autoRefreshTimer = new DispatcherTimer { Interval = TimeSpan.FromMilliseconds(AutoRefreshIntervalMs) };
            _autoRefreshTimer.Tick += async (_, _) => await AutoRefreshTickAsync();
            _autoRefreshTimer.Start();

            _ = RefreshAsync();
        }

        /// <summary>
        /// Stops the auto-refresh timer. Called when the window closes so the view model
        /// (and its engine object references) can be collected.
        /// </summary>
        public void StopAutoRefresh()
        {
            _autoRefreshTimer.Stop();
        }

        private async Task AutoRefreshTickAsync()
        {
            if (IsRefreshing)
            {
                return;
            }

            await RefreshAsync(rebuildOnlyOnChange: true).ConfigureAwait(true);
        }

        public async Task RefreshAsync(bool rebuildOnlyOnChange = false)
        {
            IsRefreshing = true;

            var snapshots = new List<LayerSnapshot>();
            var layerTypes = new List<string>();
            bool hasWorldGrid = false;

            World? skyWorld = null;
            DynamicSkySystem? skySystem = null;
            uint skyWorldId = 0;
            uint skySystemId = 0;

            World? environmentWorld = null;
            uint environmentWorldId = 0;

            await EngineManager.PostToSimThread(() =>
            {
                World? world = EngineManager.CurrentProject?.World;

                if (world == null)
                {
                    return;
                }

                environmentWorld = world;
                environmentWorldId = world.Id.Value;

                skySystem = FindDynamicSkySystem();

                if (skySystem != null)
                {
                    skyWorld = world;
                    skyWorldId = world.Id.Value;
                    skySystemId = skySystem.Id.Value;
                }

                WorldGrid? worldGrid = world.WorldGrid;

                if (worldGrid == null || !worldGrid.IsValid)
                {
                    return;
                }

                hasWorldGrid = true;

                Array layers = worldGrid.GetLayers();

                foreach (WorldGridLayer? layer in layers)
                {
                    if (layer == null || !layer.IsValid)
                    {
                        continue;
                    }

                    Name layerName = layer.GetName();
                    string typeName = layer.GetLayerClassName().ToString();
                    ObjIdBase id = layer.Id;

                    snapshots.Add(new LayerSnapshot(
                        worldGrid,
                        layer,
                        id.TypeId.Value,
                        id.Value,
                        layerName.ToString(),
                        typeName));
                }

                if (EngineManager.EditorGame?.EditorSubsystem is { } editorSubsystem)
                {
                    Array classNames = editorSubsystem.InvokeNativeMethod<Array>(new Name("GetAvailableWorldGridLayerClassNames"));

                    if (classNames != null)
                    {
                        foreach (Name className in classNames)
                        {
                            layerTypes.Add(className.ToString());
                        }
                    }
                }
            });

            await Dispatcher.UIThread.InvokeAsync(() =>
            {
                HasWorldGrid = hasWorldGrid;

                // only rebuilt when the world changes, so in-progress edits aren't disturbed
                if (environmentWorld == null)
                {
                    Environment = null;
                }
                else if (Environment == null || Environment.WorldId != environmentWorldId)
                {
                    Environment = new EnvironmentSettingsViewModel(environmentWorld, environmentWorldId);
                }

                // only rebuilt when the sky system itself changes, so in-progress edits aren't disturbed
                if (skySystem == null || skyWorld == null)
                {
                    Sky = null;
                }
                else if (Sky == null || Sky.SkySystemId != skySystemId || Sky.WorldId != skyWorldId)
                {
                    Sky = new SkySettingsViewModel(skyWorld, skyWorldId, skySystem, skySystemId);
                }

                layerTypes.Sort(StringComparer.OrdinalIgnoreCase);

                IEnumerable<string> layerTypeNames = AvailableLayerTypes.Select(t => t.ClassName);

                if (!layerTypeNames.SequenceEqual(layerTypes, StringComparer.OrdinalIgnoreCase))
                {
                    AvailableLayerTypes.Clear();

                    foreach (string typeName in layerTypes)
                    {
                        AvailableLayerTypes.Add(new WorldGridLayerTypeViewModel(typeName));
                    }
                }

                if (rebuildOnlyOnChange && !LayerListDiffers(snapshots))
                {
                    IsRefreshing = false;
                    return;
                }

                RebuildLayers(snapshots);
            });

            IsRefreshing = false;
        }

        /// <summary>
        /// The current project world's DynamicSkySystem, or null if it has none. Sim thread only.
        /// </summary>
        private static DynamicSkySystem? FindDynamicSkySystem()
        {
            if (EngineManager.EditorGame?.EditorSubsystem is not { } editorSubsystem)
            {
                return null;
            }

            DynamicSkySystem? skySystem = editorSubsystem.InvokeNativeMethod<DynamicSkySystem>(new Name("GetDynamicSkySystem"));

            return skySystem != null && skySystem.IsValid ? skySystem : null;
        }

        /// <summary>
        /// True when the engine-side layer list no longer matches what is currently shown
        /// (a layer was added/removed externally, or a layer was renamed).
        /// </summary>
        private bool LayerListDiffers(List<LayerSnapshot> snapshots)
        {
            if (Layers.Count != snapshots.Count)
            {
                return true;
            }

            for (int i = 0; i < snapshots.Count; i++)
            {
                WorldGridLayerViewModel current = Layers[i];
                LayerSnapshot snapshot = snapshots[i];

                if (current.LayerId != snapshot.LayerId
                    || current.LayerTypeId != snapshot.LayerTypeId
                    || !string.Equals(current.DisplayName, snapshot.Name, StringComparison.Ordinal))
                {
                    return true;
                }
            }

            return false;
        }

        private void RebuildLayers(List<LayerSnapshot> snapshots)
        {
            // Preserve expansion state across rebuilds, keyed by layer id.
            var expandedById = new Dictionary<(uint TypeId, uint Id), bool>();

            foreach (WorldGridLayerViewModel layer in Layers)
            {
                expandedById[(layer.LayerTypeId, layer.LayerId)] = layer.IsExpanded;
            }

            Layers.Clear();

            foreach (LayerSnapshot snapshot in snapshots)
            {
                var viewModel = new WorldGridLayerViewModel(
                    this,
                    snapshot.WorldGrid,
                    snapshot.Layer,
                    snapshot.LayerTypeId,
                    snapshot.LayerId,
                    snapshot.Name,
                    snapshot.TypeName);

                if (expandedById.TryGetValue((snapshot.LayerTypeId, snapshot.LayerId), out bool isExpanded))
                {
                    viewModel.IsExpanded = isExpanded;
                }

                Layers.Add(viewModel);
            }

            ApplyFilter();
        }

        private async Task AddLayerAsync(string? typeName)
        {
            if (string.IsNullOrEmpty(typeName))
            {
                return;
            }

            EngineManager.EditorGame?.EditorSubsystem?.ExecuteCommandByName(new Name("EditorCommandAddWorldGridLayer"), typeName);

            // The command is enqueued on the sim thread; this refresh is posted after it,
            // so it will observe the newly added layer.
            await RefreshAsync();
        }

        private async Task RemoveLayerAsync(WorldGridLayerViewModel? layer)
        {
            if (layer == null)
            {
                return;
            }

            WorldGrid worldGrid = layer.WorldGrid;
            WorldGridLayer worldGridLayer = layer.Layer;
            string label = string.IsNullOrWhiteSpace(layer.DisplayName) ? layer.TypeName : layer.DisplayName.Trim();

            await EngineManager.PostToSimThread(() =>
            {
                if (!worldGrid.IsValid || !worldGridLayer.IsValid)
                {
                    return;
                }

                worldGrid.RemoveLayer(worldGridLayer);

                EditorProject? project = EngineManager.CurrentProject;

                project?.ActionStack?.PushAction(new EditorAction(
                    $"Remove World Grid Layer: {label}",
                    execute: (_, _) => worldGrid.RemoveLayer(worldGridLayer),
                    revert: (_, _) => worldGrid.AddLayer(worldGridLayer)));
            });

            await RefreshAsync();
        }

        private void ApplyFilter()
        {
            string? search = SearchText?.Trim();

            IEnumerable<WorldGridLayerViewModel> filtered = Layers;

            if (!string.IsNullOrEmpty(search))
            {
                filtered = Layers.Where(layer =>
                    layer.DisplayName.Contains(search, StringComparison.OrdinalIgnoreCase)
                    || layer.TypeName.Contains(search, StringComparison.OrdinalIgnoreCase));
            }

            FilteredLayers.Clear();

            foreach (WorldGridLayerViewModel layer in filtered)
            {
                FilteredLayers.Add(layer);
            }

            OnPropertyChanged(nameof(HasLayers));
            OnPropertyChanged(nameof(ShowNoLayersHint));
        }

        private sealed class LayerSnapshot
        {
            public WorldGrid WorldGrid { get; }
            public WorldGridLayer Layer { get; }
            public uint LayerTypeId { get; }
            public uint LayerId { get; }
            public string Name { get; }
            public string TypeName { get; }

            public LayerSnapshot(
                WorldGrid worldGrid,
                WorldGridLayer layer,
                uint layerTypeId,
                uint layerId,
                string name,
                string typeName)
            {
                WorldGrid = worldGrid;
                Layer = layer;
                LayerTypeId = layerTypeId;
                LayerId = layerId;
                Name = name;
                TypeName = typeName;
            }
        }
    }

    /// <summary>
    /// A concrete WorldGridLayer-derived class that can be added to the world grid.
    /// </summary>
    public class WorldGridLayerTypeViewModel : ViewModelBase
    {
        public string ClassName { get; }

        public string DisplayName { get; }

        public string IconKind { get; }

        public WorldGridLayerTypeViewModel(string className)
        {
            ClassName = className ?? throw new ArgumentNullException(nameof(className));

            const string suffix = "WorldGridLayer";

            DisplayName = ClassName.EndsWith(suffix, StringComparison.Ordinal)
                ? ClassName[..^suffix.Length]
                : ClassName;

            IconKind = DisplayName.Contains("Terrain", StringComparison.OrdinalIgnoreCase) ? "Landscape" : "Layers";
        }
    }
}
