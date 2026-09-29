using System;
using System.Collections.Generic;
using System.Collections.ObjectModel;
using Avalonia.Threading;
using Hyperion;

namespace Hyperion.Editor.ViewModels
{
    public class TerrainPaintPanelViewModel : EditorPanelViewModel
    {
        private const int TerrainLayerTarget = 0;
        private const int GroundCoverTarget = 1;

        private readonly EditorTerrainState _terrainState;

        private int _paintTargetIndex = TerrainLayerTarget;
        public int PaintTargetIndex
        {
            get => _paintTargetIndex;
            set
            {
                if (SetProperty(ref _paintTargetIndex, value))
                {
                    _terrainState.SetMode(value == GroundCoverTarget ? TerrainSculptMode.PaintGroundCover : TerrainSculptMode.PaintSplat);

                    OnPropertyChanged(nameof(IsTerrainLayerTarget));
                    OnPropertyChanged(nameof(IsGroundCoverTarget));

                    if (value == GroundCoverTarget)
                    {
                        RefreshGroundCoverLayers();
                    }
                }
            }
        }

        public bool IsTerrainLayerTarget => _paintTargetIndex == TerrainLayerTarget;

        public bool IsGroundCoverTarget => _paintTargetIndex == GroundCoverTarget;

        private int _paintLayerIndex = 0;
        public int PaintLayerIndex
        {
            get => _paintLayerIndex;
            set
            {
                if (SetProperty(ref _paintLayerIndex, value))
                {
                    _terrainState.SetPaintLayer(value);
                }
            }
        }

        public ObservableCollection<string> GroundCoverLayers { get; } = new ObservableCollection<string>();

        private string? _selectedGroundCoverLayer;
        public string? SelectedGroundCoverLayer
        {
            get => _selectedGroundCoverLayer;
            set
            {
                if (SetProperty(ref _selectedGroundCoverLayer, value) && !string.IsNullOrEmpty(value))
                {
                    _terrainState.SetPaintGroundCoverLayer(new Name(value));
                }
            }
        }

        public bool HasGroundCoverLayers => GroundCoverLayers.Count != 0;

        private double _radius = 5.0;
        public double Radius
        {
            get => _radius;
            set
            {
                if (SetProperty(ref _radius, value))
                {
                    _terrainState.SetRadius((float)value);
                    OnPropertyChanged(nameof(RadiusText));
                }
            }
        }

        public string RadiusText => $"{_radius:0.#} m";

        private double _strength = 2.0;
        public double Strength
        {
            get => _strength;
            set
            {
                if (SetProperty(ref _strength, value))
                {
                    _terrainState.SetStrength((float)value);
                    OnPropertyChanged(nameof(StrengthText));
                }
            }
        }

        public string StrengthText => $"{_strength:0.00}";

        public TerrainPaintPanelViewModel(EditorTerrainState terrainState, Action? onClosed)
            : base("Terrain Painting", onClosed)
        {
            _terrainState = terrainState ?? throw new ArgumentNullException(nameof(terrainState));

            _paintLayerIndex = terrainState.GetPaintLayer();
            _paintTargetIndex = terrainState.GetMode() == TerrainSculptMode.PaintGroundCover ? GroundCoverTarget : TerrainLayerTarget;

            RefreshGroundCoverLayers();
        }

        private void RefreshGroundCoverLayers()
        {
            _ = EngineManager.PostToSimThread(() =>
            {
                var layerNames = new List<string>();

                foreach (Name layerName in _terrainState.GetPaintableGroundCoverLayers())
                {
                    layerNames.Add(layerName.ToString());
                }

                string currentLayer = _terrainState.GetPaintGroundCoverLayer().ToString();

                Dispatcher.UIThread.Post(() =>
                {
                    GroundCoverLayers.Clear();

                    foreach (string layerName in layerNames)
                    {
                        GroundCoverLayers.Add(layerName);
                    }

                    OnPropertyChanged(nameof(HasGroundCoverLayers));

                    SelectedGroundCoverLayer = layerNames.Contains(currentLayer)
                        ? currentLayer
                        : (layerNames.Count != 0 ? layerNames[0] : null);
                });
            });
        }
    }
}
