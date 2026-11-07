using System;
using Hyperion;

namespace Hyperion.Editor.ViewModels
{
    public class TerrainSculptPanelViewModel : EditorPanelViewModel
    {
        private readonly EditorTerrainState _terrainState;

        private TerrainSculptMode _mode;

        public bool IsRaiseMode
        {
            get => _mode == TerrainSculptMode.Raise;
            set => CheckMode(TerrainSculptMode.Raise, value);
        }

        public bool IsLowerMode
        {
            get => _mode == TerrainSculptMode.Lower;
            set => CheckMode(TerrainSculptMode.Lower, value);
        }

        public bool IsFlattenMode
        {
            get => _mode == TerrainSculptMode.Flatten;
            set => CheckMode(TerrainSculptMode.Flatten, value);
        }

        public bool IsSmoothMode
        {
            get => _mode == TerrainSculptMode.Smooth;
            set => CheckMode(TerrainSculptMode.Smooth, value);
        }

        public bool IsRampMode
        {
            get => _mode == TerrainSculptMode.Ramp;
            set => CheckMode(TerrainSculptMode.Ramp, value);
        }

        public bool IsProceduralMode
        {
            get => _mode == TerrainSculptMode.MarkProcedural;
            set => CheckMode(TerrainSculptMode.MarkProcedural, value);
        }

        public bool HasFalloff => IsFlattenMode || IsRampMode;

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

        private double _falloff;
        public double Falloff
        {
            get => _falloff;
            set
            {
                if (SetProperty(ref _falloff, value))
                {
                    _terrainState.SetFalloff((float)value);
                }
            }
        }

        public TerrainSculptPanelViewModel(EditorTerrainState terrainState, Action? onClosed)
            : base("Terrain Sculpting", onClosed)
        {
            _terrainState = terrainState ?? throw new ArgumentNullException(nameof(terrainState));

            _mode = terrainState.GetMode();
            _radius = terrainState.Radius;
            _strength = terrainState.Strength;
            _falloff = terrainState.GetFalloff();

            if (_mode == TerrainSculptMode.PaintSplat || _mode == TerrainSculptMode.PaintGroundCover)
            {
                _mode = TerrainSculptMode.Raise;
            }
        }

        private void CheckMode(TerrainSculptMode mode, bool isChecked)
        {
            if (!isChecked || _mode == mode)
            {
                return;
            }

            _mode = mode;

            _terrainState.SetMode(mode);

            OnPropertyChanged(nameof(IsRaiseMode));
            OnPropertyChanged(nameof(IsLowerMode));
            OnPropertyChanged(nameof(IsFlattenMode));
            OnPropertyChanged(nameof(IsSmoothMode));
            OnPropertyChanged(nameof(IsRampMode));
            OnPropertyChanged(nameof(IsProceduralMode));
            OnPropertyChanged(nameof(HasFalloff));
        }
    }
}
