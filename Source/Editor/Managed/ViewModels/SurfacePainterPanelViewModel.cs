using System;
using System.Collections.Generic;
using System.Collections.ObjectModel;
using System.Linq;
using System.Windows.Input;
using Hyperion;
using Hyperion.Editor.Commands;

namespace Hyperion.Editor.ViewModels
{
    public abstract class SurfacePainterPanelViewModel : EditorPanelViewModel
    {
        private readonly EditorSurfacePainterState _painterState;

        private readonly PainterAssetPicker _assetPicker;

        public string AssetLabel { get; }

        public virtual bool HasGroundFitting => false;

        public ObservableCollection<InspectorPropertyViewModelBase> Properties => _assetPicker.Properties;

        private double _scale;
        public double Scale
        {
            get => _scale;
            set
            {
                if (SetProperty(ref _scale, value))
                {
                    _painterState.SetScale((float)value);
                    OnPropertyChanged(nameof(ScaleText));
                }
            }
        }

        public string ScaleText => $"{_scale:0.00}x";

        private double _rotationDegrees;
        public double RotationDegrees
        {
            get => _rotationDegrees;
            set
            {
                if (SetProperty(ref _rotationDegrees, value))
                {
                    _painterState.SetRotationDegrees((float)value);
                    OnPropertyChanged(nameof(RotationText));
                }
            }
        }

        public string RotationText => $"{_rotationDegrees:0}°";

        private bool _randomRotation;
        public bool RandomRotation
        {
            get => _randomRotation;
            set
            {
                if (SetProperty(ref _randomRotation, value))
                {
                    _painterState.SetRandomRotation(value);
                }
            }
        }

        private double _spacing;
        public double Spacing
        {
            get => _spacing;
            set
            {
                if (SetProperty(ref _spacing, value))
                {
                    _painterState.SetSpacing((float)value);
                    OnPropertyChanged(nameof(SpacingText));
                }
            }
        }

        public string SpacingText => _spacing <= 0.0 ? "Single stamp" : $"{_spacing:0.0#} m";

        private double _eraseRadius;
        public double EraseRadius
        {
            get => _eraseRadius;
            set
            {
                if (SetProperty(ref _eraseRadius, value))
                {
                    _painterState.SetEraseRadius((float)value);
                    OnPropertyChanged(nameof(EraseRadiusText));
                }
            }
        }

        public string EraseRadiusText => $"{_eraseRadius:0.0#} m";

        private bool _alignToSurface;
        public bool AlignToSurface
        {
            get => _alignToSurface;
            set
            {
                if (SetProperty(ref _alignToSurface, value))
                {
                    _painterState.SetAlignToSurface(value);
                }
            }
        }

        protected SurfacePainterPanelViewModel(
            string title,
            string assetLabel,
            string activeAssetPropertyName,
            AssetBucket assetBucket,
            EditorSubsystem? editorSubsystem,
            EditorSurfacePainterState? painterState,
            Action? onClosed)
            : base(title)
        {
            ArgumentNullException.ThrowIfNull(editorSubsystem);
            ArgumentNullException.ThrowIfNull(painterState);

            _painterState = painterState;

            AssetLabel = assetLabel;

            _scale = painterState.Scale;
            _rotationDegrees = painterState.RotationDegrees;
            _randomRotation = painterState.RandomRotation;
            _spacing = painterState.Spacing;
            _eraseRadius = painterState.EraseRadius;
            _alignToSurface = painterState.AlignToSurface;

            _assetPicker = new PainterAssetPicker(title, painterState, activeAssetPropertyName, assetBucket, editorSubsystem);
            _assetPicker.AssetBucketChanged += OnAssetBucketChanged;

            OnClosed = () =>
            {
                _assetPicker.Dispose();

                onClosed?.Invoke();
            };
        }

        /// <summary>Sim thread. Called when the painter's asset bucket changes, before the picker refreshes.</summary>
        protected virtual void OnAssetBucketChanged()
        {
        }

        protected HashSet<string> GetAssetNames()
        {
            return _assetPicker.GetAssetNames();
        }
    }

    public class DecalPainterPanelViewModel : SurfacePainterPanelViewModel
    {
        private readonly EditorDecalPainterState _decalPainterState;

        private HashSet<string>? _decalNamesBeforeCreate;

        public ICommand NewDecalCommand { get; }

        public DecalPainterPanelViewModel(EditorSubsystem? editorSubsystem, EditorDecalPainterState? painterState, ICommand? newDecalCommand, Action? onClosed)
            : base("Decal Painter", "Decal", "ActiveDecal", AssetBucket.Decals, editorSubsystem, painterState, onClosed)
        {
            ArgumentNullException.ThrowIfNull(newDecalCommand);

            _decalPainterState = painterState!;

            // same command as the content browser's New > Decal
            NewDecalCommand = new RelayCommand(() =>
            {
                // queued ahead of the command's own sim thread work, so this sees the bucket before the new asset lands
                _ = EngineManager.PostToSimThread(() =>
                {
                    _decalNamesBeforeCreate = GetAssetNames();
                });

                newDecalCommand.Execute(null);
            }, () => newDecalCommand.CanExecute(null));
        }

        protected override void OnAssetBucketChanged()
        {
            if (_decalNamesBeforeCreate == null)
            {
                return;
            }

            string? createdName = GetAssetNames().FirstOrDefault(name => !_decalNamesBeforeCreate.Contains(name));

            if (createdName != null)
            {
                _decalNamesBeforeCreate = null;

                _decalPainterState.SetActiveDecalByName(new Name(createdName));
            }
        }
    }

    public class InstancePainterPanelViewModel : SurfacePainterPanelViewModel
    {
        private readonly EditorInstancePainterState _instancePainterState;

        public override bool HasGroundFitting => true;

        private double _footprintRadius;
        public double FootprintRadius
        {
            get => _footprintRadius;
            set
            {
                if (SetProperty(ref _footprintRadius, value))
                {
                    _instancePainterState.SetFootprintRadius((float)value);
                    OnPropertyChanged(nameof(FootprintRadiusText));
                }
            }
        }

        public string FootprintRadiusText => _footprintRadius <= 0.0 ? "Off" : $"{_footprintRadius:0.0#} m";

        private double _sinkDepth;
        public double SinkDepth
        {
            get => _sinkDepth;
            set
            {
                if (SetProperty(ref _sinkDepth, value))
                {
                    _instancePainterState.SetSinkDepth((float)value);
                    OnPropertyChanged(nameof(SinkDepthText));
                }
            }
        }

        public string SinkDepthText => $"{_sinkDepth:0.0#} m";

        public InstancePainterPanelViewModel(EditorSubsystem? editorSubsystem, EditorInstancePainterState? painterState, Action? onClosed)
            : base("Instance Painter", "Prefab", "ActivePrefab", AssetBucket.Prefabs, editorSubsystem, painterState, onClosed)
        {
            _instancePainterState = painterState!;

            _footprintRadius = painterState!.FootprintRadius;
            _sinkDepth = painterState.SinkDepth;
        }
    }
}
