using System;
using System.Collections.Generic;
using System.Collections.ObjectModel;
using System.Linq;
using System.Windows.Input;
using Avalonia.Threading;
using Hyperion;
using Hyperion.Editor.Commands;

namespace Hyperion.Editor.ViewModels
{
    public class DecalPainterPanelViewModel : EditorPanelViewModel
    {
        private readonly EditorDecalPainterState _painterState;

        private DelegateHandler? _onAssetsChangedHandler;

        // sim thread only - the Decals bucket as it was when New Decal was clicked here, so the created asset can be picked out and made active
        private HashSet<string>? _decalNamesBeforeCreate;

        private readonly InspectorPropertyViewModelBase? _activeDecalProperty;

        /// <summary>The painter's ActiveDecal property, shown with the inspector's asset picker.</summary>
        public ObservableCollection<InspectorPropertyViewModelBase> Properties { get; } = new ObservableCollection<InspectorPropertyViewModelBase>();

        private double _scale = 1.0;
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

        private double _rotationDegrees = 0.0;
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

        private bool _randomRotation = true;
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

        private double _spacing = 0.5;
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

        private double _eraseRadius = 1.0;
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

        private bool _alignToSurface = true;
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

        public ICommand NewDecalCommand { get; }

        public DecalPainterPanelViewModel(EditorSubsystem? editorSubsystem, EditorDecalPainterState? painterState, ICommand? newDecalCommand, Action? onClosed)
            : base("Decal Painter")
        {
            ArgumentNullException.ThrowIfNull(editorSubsystem);
            ArgumentNullException.ThrowIfNull(newDecalCommand);
            ArgumentNullException.ThrowIfNull(painterState);

            _painterState = painterState;

            _scale = painterState.Scale;
            _rotationDegrees = painterState.RotationDegrees;
            _randomRotation = painterState.RandomRotation;
            _spacing = painterState.Spacing;
            _eraseRadius = painterState.EraseRadius;
            _alignToSurface = painterState.AlignToSurface;

            Property? activeDecalProperty = painterState.Class.GetProperty(new Name("ActiveDecal"));

            if (activeDecalProperty != null)
            {
                _activeDecalProperty = InspectorViewModelFactory.Create(painterState, activeDecalProperty.Value, isReadOnly: false);

                Properties.Add(_activeDecalProperty);
            }
            else
            {
                Logger.Log(LogLevel.Error, "Decal painter: EditorDecalPainterState has no ActiveDecal property");
            }

            // same command as the content browser's New > Decal
            NewDecalCommand = new RelayCommand(() =>
            {
                // queued ahead of the command's own sim thread work, so this sees the bucket before the new asset lands
                _ = EngineManager.PostToSimThread(() =>
                {
                    _decalNamesBeforeCreate = GetDecalNames();
                });

                newDecalCommand.Execute(null);
            }, () => newDecalCommand.CanExecute(null));

            // sim thread
            _onAssetsChangedHandler = editorSubsystem.GetOnAssetsChangedDelegate().Bind((uint bucketIndex) =>
            {
                if (bucketIndex != AssetBucket.Decals.Value)
                {
                    return;
                }

                if (_decalNamesBeforeCreate != null)
                {
                    string? createdName = GetDecalNames().FirstOrDefault(name => !_decalNamesBeforeCreate.Contains(name));

                    if (createdName != null)
                    {
                        _decalNamesBeforeCreate = null;

                        _painterState.SetActiveDecalByName(new Name(createdName));
                    }
                }

                // the active decal may have just been created, or deleted out from under the painter
                Dispatcher.UIThread.Post(() => _activeDecalProperty?.RefreshValue());
            });

            OnClosed = () =>
            {
                _onAssetsChangedHandler?.Remove();
                _onAssetsChangedHandler?.Dispose();
                _onAssetsChangedHandler = null;

                onClosed?.Invoke();
            };
        }

        private static HashSet<string> GetDecalNames()
        {
            AssetRegistry registry = AssetManager.Instance.AssetRegistry;

            return new HashSet<string>(
                registry.GetBucketAssetDescs(AssetBucket.Decals.Value).Select(assetDesc => assetDesc.Name.ToString()),
                StringComparer.Ordinal);
        }
    }
}
