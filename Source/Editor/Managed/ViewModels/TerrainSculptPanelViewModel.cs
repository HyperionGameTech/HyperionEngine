using System;
using System.Windows.Input;
using Hyperion;
using Hyperion.Editor.Commands;

namespace Hyperion.Editor.ViewModels
{
    public class TerrainSculptPanelViewModel : EditorPanelViewModel
    {
        private readonly EditorTerrainState _terrainState;

        private TerrainSculptMode _mode;

        public bool IsRaiseMode => _mode == TerrainSculptMode.Raise;
        public bool IsLowerMode => _mode == TerrainSculptMode.Lower;
        public bool IsProceduralMode => _mode == TerrainSculptMode.MarkProcedural;

        public ICommand SetRaiseModeCommand { get; }
        public ICommand SetLowerModeCommand { get; }
        public ICommand SetProceduralModeCommand { get; }

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

        public TerrainSculptPanelViewModel(EditorTerrainState terrainState, Action? onClosed)
            : base("Terrain Sculpting", onClosed)
        {
            _terrainState = terrainState ?? throw new ArgumentNullException(nameof(terrainState));

            SetRaiseModeCommand = new RelayCommand(() => SetMode(TerrainSculptMode.Raise));
            SetLowerModeCommand = new RelayCommand(() => SetMode(TerrainSculptMode.Lower));
            SetProceduralModeCommand = new RelayCommand(() => SetMode(TerrainSculptMode.MarkProcedural));

            _mode = terrainState.GetMode();

            if (_mode != TerrainSculptMode.Lower && _mode != TerrainSculptMode.MarkProcedural)
            {
                _mode = TerrainSculptMode.Raise;
            }
        }

        private void SetMode(TerrainSculptMode mode)
        {
            if (_mode == mode)
            {
                return;
            }

            _mode = mode;

            _terrainState.SetMode(mode);

            OnPropertyChanged(nameof(IsRaiseMode));
            OnPropertyChanged(nameof(IsLowerMode));
            OnPropertyChanged(nameof(IsProceduralMode));
        }
    }
}
