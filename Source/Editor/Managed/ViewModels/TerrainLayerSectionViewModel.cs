using System;
using System.Collections.Generic;
using System.Collections.ObjectModel;
using System.Windows.Input;
using Avalonia.Threading;
using Hyperion;
using Hyperion.Editor.Commands;

namespace Hyperion.Editor.ViewModels
{
    public class TerrainLayerSectionViewModel : ViewModelBase
    {
        private static readonly HashSet<string> s_sectionPropertyNames = new HashSet<string>
        {
            "HeightSource",
            "FlatHeight",
            "Heightmap",
            "HeightmapTiles",
            "HeightmapCenter",
            "HeightmapSize",
            "HeightmapHeightRange",
            "ProceduralEdges",
            "BlendDistance"
        };

        private readonly TerrainWorldGridLayer _layer;
        private readonly Dictionary<string, Property> _properties = new();

        public ObservableCollection<InspectorPropertyViewModelBase> SourceProperties { get; } = new();
        public ObservableCollection<InspectorPropertyViewModelBase> FlatProperties { get; } = new();
        public ObservableCollection<InspectorPropertyViewModelBase> HeightmapProperties { get; } = new();
        public ObservableCollection<InspectorPropertyViewModelBase> HeightmapTileProperties { get; } = new();
        public ObservableCollection<InspectorPropertyViewModelBase> PlacementProperties { get; } = new();

        public ICommand ImportHeightmapTilesCommand { get; } = new EditorCommand("ImportTerrainHeightmapTiles");
        public ICommand ClearProceduralCellsCommand { get; }

        private TerrainHeightSource _heightSource = TerrainHeightSource.Procedural;

        public bool IsFlat => _heightSource == TerrainHeightSource.Flat;
        public bool IsHeightmap => _heightSource == TerrainHeightSource.Heightmap;
        public bool HasProceduralCellsSection => _heightSource != TerrainHeightSource.Procedural;

        private uint _numHeightmapTiles;

        public bool IsTiledHeightmap => IsHeightmap && _numHeightmapTiles > 1;
        public bool IsSingleHeightmap => IsHeightmap && _numHeightmapTiles <= 1;

        private uint _numProceduralCells;

        public bool HasProceduralCells => _numProceduralCells > 0;

        public string ProceduralCellsText => _numProceduralCells switch
        {
            0 => "None",
            1 => "1 cell",
            _ => $"{_numProceduralCells} cells"
        };

        public TerrainLayerSectionViewModel(TerrainWorldGridLayer layer, IEnumerable<Property> layerProperties)
        {
            _layer = layer ?? throw new ArgumentNullException(nameof(layer));

            ClearProceduralCellsCommand = new RelayCommand(ClearProceduralCells);

            foreach (Property property in layerProperties)
            {
                if (IsSectionProperty(property))
                {
                    _properties[property.Name.ToString()] = property;
                }
            }

            AddProperty(SourceProperties, "HeightSource");

            AddProperty(FlatProperties, "FlatHeight");
            AddProperty(FlatProperties, "BlendDistance");

            AddProperty(HeightmapProperties, "Heightmap");
            AddProperty(HeightmapTileProperties, "HeightmapTiles");

            AddProperty(PlacementProperties, "HeightmapCenter");
            AddProperty(PlacementProperties, "HeightmapSize");
            AddProperty(PlacementProperties, "HeightmapHeightRange");
            AddProperty(PlacementProperties, "ProceduralEdges");
            AddProperty(PlacementProperties, "BlendDistance");

            Refresh();
        }

        public static bool IsSectionProperty(Property property)
        {
            return s_sectionPropertyNames.Contains(property.Name.ToString());
        }

        public void Refresh()
        {
            _ = EngineManager.PostToSimThread(() =>
            {
                if (!_layer.IsValid)
                {
                    return;
                }

                TerrainHeightSource heightSource = _layer.GetHeightSource();
                uint numHeightmapTiles = _layer.GetNumHeightmapTiles();
                uint numProceduralCells = _layer.GetNumProceduralCells();

                Dispatcher.UIThread.Post(() => Apply(heightSource, numHeightmapTiles, numProceduralCells));
            });
        }

        private void Apply(TerrainHeightSource heightSource, uint numHeightmapTiles, uint numProceduralCells)
        {
            bool tilesChanged = _numHeightmapTiles != numHeightmapTiles;

            if (!tilesChanged && _heightSource == heightSource && _numProceduralCells == numProceduralCells)
            {
                return;
            }

            _heightSource = heightSource;
            _numHeightmapTiles = numHeightmapTiles;
            _numProceduralCells = numProceduralCells;

            OnPropertyChanged(nameof(IsFlat));
            OnPropertyChanged(nameof(IsHeightmap));
            OnPropertyChanged(nameof(HasProceduralCellsSection));
            OnPropertyChanged(nameof(IsTiledHeightmap));
            OnPropertyChanged(nameof(IsSingleHeightmap));
            OnPropertyChanged(nameof(HasProceduralCells));
            OnPropertyChanged(nameof(ProceduralCellsText));

            // the other source's editors may be showing values from before the change
            if (tilesChanged)
            {
                RefreshValues(HeightmapProperties);
                RefreshValues(HeightmapTileProperties);
            }

            RefreshValues(SourceProperties);
        }

        private static void RefreshValues(IEnumerable<InspectorPropertyViewModelBase> viewModels)
        {
            foreach (InspectorPropertyViewModelBase viewModel in viewModels)
            {
                viewModel.RefreshValue();
            }
        }

        private void AddProperty(ObservableCollection<InspectorPropertyViewModelBase> group, string propertyName)
        {
            if (!_properties.TryGetValue(propertyName, out Property property))
            {
                return;
            }

            try
            {
                group.Add(InspectorViewModelFactory.Create(
                    _layer,
                    property,
                    isReadOnly: false,
                    valueChangedCallback: Refresh));
            }
            catch (Exception ex)
            {
                Logger.Log(LogLevel.Warning, $"Terrain settings: skipping property '{propertyName}': {ex.Message}");
            }
        }

        private void ClearProceduralCells()
        {
            _ = EngineManager.PostToSimThread(() =>
            {
                if (_layer.IsValid)
                {
                    _layer.ClearProceduralCells();
                }
            }).ContinueWith(_ => Refresh());
        }
    }
}
