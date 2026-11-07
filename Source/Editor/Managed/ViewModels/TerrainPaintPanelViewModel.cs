using System;
using Hyperion;

namespace Hyperion.Editor.ViewModels
{
    public class TerrainPaintPanelViewModel : EditorPanelViewModel
    {
        private readonly EditorTerrainState _terrainState;

        private bool _eraseMode;

        public bool PaintMode
        {
            get => !_eraseMode;
            set
            {
                if (value)
                {
                    SetEraseMode(false);
                }
            }
        }

        public bool EraseMode
        {
            get => _eraseMode;
            set
            {
                if (value)
                {
                    SetEraseMode(true);
                }
            }
        }

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

        private double _radius;
        public double Radius
        {
            get => _radius;
            set
            {
                if (SetProperty(ref _radius, value))
                {
                    _terrainState.SetRadius((float)value);
                }
            }
        }

        private double _strength;
        public double Strength
        {
            get => _strength;
            set
            {
                if (SetProperty(ref _strength, value))
                {
                    _terrainState.SetStrength((float)value);
                }
            }
        }

        public TerrainPaintPanelViewModel(EditorTerrainState terrainState, Action? onClosed)
            : base("Terrain Painting", onClosed)
        {
            _terrainState = terrainState ?? throw new ArgumentNullException(nameof(terrainState));

            _paintLayerIndex = terrainState.GetPaintLayer();
            _radius = terrainState.Radius;
            _strength = terrainState.Strength;
            _eraseMode = terrainState.EraseMode;
        }

        private void SetEraseMode(bool eraseMode)
        {
            if (_eraseMode == eraseMode)
            {
                return;
            }

            _eraseMode = eraseMode;
            _terrainState.SetEraseMode(eraseMode);

            OnPropertyChanged(nameof(PaintMode));
            OnPropertyChanged(nameof(EraseMode));
        }
    }
}
