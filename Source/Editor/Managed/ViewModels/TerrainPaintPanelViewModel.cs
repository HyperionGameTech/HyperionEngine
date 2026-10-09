using System;
using Hyperion;

namespace Hyperion.Editor.ViewModels
{
    public class TerrainPaintPanelViewModel : EditorPanelViewModel
    {
        private readonly EditorTerrainState _terrainState;

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
        }
    }
}
