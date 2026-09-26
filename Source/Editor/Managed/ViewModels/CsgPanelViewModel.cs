using System;
using System.Windows.Input;
using Avalonia.Threading;
using Hyperion;
using Hyperion.Editor.Commands;

namespace Hyperion.Editor.ViewModels
{
    public struct CsgStateSnapshot
    {
        public bool Enabled;
        public bool CanEnable;
        public bool Simulating;
        public bool HasBrush;
        public bool IsBrushSelected;
        public bool IsPlacing;
        public CsgBrushShape PlacementShape = CsgBrushShape.Box;
        public bool CanApply;
        public bool HasPendingEdits;
        public bool KeepBrushAfterApply;
        public CsgBrushShape BrushShape = CsgBrushShape.Box;
        public CsgOperation Operation = CsgOperation.Subtract;
        public string TargetName = string.Empty;
        public string StatusText = string.Empty;
        public CsgBrushShape SizingShape = CsgBrushShape.Box;
        public Vec3f SizingHalfExtents = new Vec3f(0.5f);

        public CsgStateSnapshot()
        {
        }
    }

    public class CsgPanelViewModel : EditorPanelViewModel
    {
        private readonly EditorSubsystem _editorSubsystem;
        private readonly Action _refreshState;

        private CsgStateSnapshot _state = new CsgStateSnapshot();

        public string TargetName => string.IsNullOrEmpty(_state.TargetName) ? "(no mesh)" : _state.TargetName;

        public string StatusText => _state.StatusText;
        public bool HasStatusText => !string.IsNullOrEmpty(_state.StatusText);

        public bool HasBrush => _state.HasBrush;
        public bool IsBrushSelected => _state.IsBrushSelected;

        public bool IsPlacing => _state.IsPlacing;

        private bool IsShapeActive(CsgBrushShape shape) => _state.IsPlacing
            ? _state.PlacementShape == shape
            : _state.HasBrush && _state.BrushShape == shape;

        public bool IsBoxShape => IsShapeActive(CsgBrushShape.Box);
        public bool IsSphereShape => IsShapeActive(CsgBrushShape.Sphere);
        public bool IsCylinderShape => IsShapeActive(CsgBrushShape.Cylinder);

        public bool ShowSizing => _state.HasBrush || _state.IsPlacing;
        public bool IsSizingBox => ShowSizing && _state.SizingShape == CsgBrushShape.Box;
        public bool IsSizingSphere => ShowSizing && _state.SizingShape == CsgBrushShape.Sphere;
        public bool IsSizingCylinder => ShowSizing && _state.SizingShape == CsgBrushShape.Cylinder;
        public bool IsSizingRound => IsSizingSphere || IsSizingCylinder;

        public float BoxSizeX
        {
            get => RoundForDisplay(_state.SizingHalfExtents.X * 2.0f);
            set => PushSizingHalfExtents(new Vec3f(value * 0.5f, _state.SizingHalfExtents.Y, _state.SizingHalfExtents.Z));
        }

        public float BoxSizeY
        {
            get => RoundForDisplay(_state.SizingHalfExtents.Y * 2.0f);
            set => PushSizingHalfExtents(new Vec3f(_state.SizingHalfExtents.X, value * 0.5f, _state.SizingHalfExtents.Z));
        }

        public float BoxSizeZ
        {
            get => RoundForDisplay(_state.SizingHalfExtents.Z * 2.0f);
            set => PushSizingHalfExtents(new Vec3f(_state.SizingHalfExtents.X, _state.SizingHalfExtents.Y, value * 0.5f));
        }

        public float Radius
        {
            get => RoundForDisplay(_state.SizingHalfExtents.X);
            set => PushSizingHalfExtents(_state.SizingShape == CsgBrushShape.Sphere
                ? new Vec3f(value)
                : new Vec3f(value, _state.SizingHalfExtents.Y, value));
        }

        public float Height
        {
            get => RoundForDisplay(_state.SizingHalfExtents.Y * 2.0f);
            set => PushSizingHalfExtents(new Vec3f(_state.SizingHalfExtents.X, value * 0.5f, _state.SizingHalfExtents.Z));
        }

        public bool IsUnion => _state.Operation == CsgOperation.Union;
        public bool IsSubtract => _state.Operation == CsgOperation.Subtract;
        public bool IsIntersect => _state.Operation == CsgOperation.Intersect;

        public string OperationDescription => _state.Operation switch
        {
            CsgOperation.Union => "Adds the brush volume to the mesh.",
            CsgOperation.Subtract => "Carves the brush volume out of the mesh.",
            CsgOperation.Intersect => "Keeps only the part of the mesh inside the brush.",
            _ => string.Empty
        };

        public string ApplyText => _state.Operation switch
        {
            CsgOperation.Union => "Apply Union",
            CsgOperation.Subtract => "Apply Subtract",
            CsgOperation.Intersect => "Apply Intersect",
            _ => "Apply"
        };

        public bool HasPendingEdits => _state.HasPendingEdits;

        private bool _keepBrushAfterApply;
        public bool KeepBrushAfterApply
        {
            get => _keepBrushAfterApply;
            set
            {
                if (SetProperty(ref _keepBrushAfterApply, value))
                {
                    _ = EngineManager.PostToSimThread(() =>
                    {
                        _editorSubsystem.EditorCsgState?.SetKeepBrushAfterApply(value);

                        _refreshState();
                    });
                }
            }
        }

        public ICommand SelectShapeCommand { get; }
        public ICommand SetOperationCommand { get; }
        public ICommand ApplyCommand { get; }
        public ICommand CancelPlacementCommand { get; }
        public ICommand SelectBrushCommand { get; }
        public ICommand ResetBrushCommand { get; }
        public ICommand RemoveBrushCommand { get; }
        public ICommand FinishCommand { get; }
        public ICommand DiscardCommand { get; }

        public CsgPanelViewModel(EditorSubsystem editorSubsystem, Action refreshState, Action? onClosed)
            : base("Brush Edits", onClosed)
        {
            _editorSubsystem = editorSubsystem ?? throw new ArgumentNullException(nameof(editorSubsystem));
            _refreshState = refreshState ?? throw new ArgumentNullException(nameof(refreshState));

            SelectShapeCommand = new RelayCommand<object?>(parameter =>
            {
                if (!Enum.TryParse(parameter?.ToString(), out CsgBrushShape shape))
                {
                    return;
                }

                PostToCsgState(csgState => csgState.SelectBrushShape(shape));
            });

            SetOperationCommand = new RelayCommand<object?>(parameter =>
            {
                if (!Enum.TryParse(parameter?.ToString(), out CsgOperation operation))
                {
                    return;
                }

                PostToCsgState(csgState => csgState.SetOperation(operation));
            });

            ApplyCommand = new RelayCommand(() => PostToCsgState(csgState => csgState.ApplyBrush()), () => _state.CanApply);
            CancelPlacementCommand = new RelayCommand(() => PostToCsgState(csgState => csgState.CancelPlacement()), () => _state.IsPlacing);
            SelectBrushCommand = new RelayCommand(() => PostToCsgState(csgState => csgState.SelectBrush()), () => _state.HasBrush);
            ResetBrushCommand = new RelayCommand(() => PostToCsgState(csgState => csgState.ResetBrush()), () => _state.HasBrush);
            RemoveBrushCommand = new RelayCommand(() => PostToCsgState(csgState => csgState.CancelBrush()), () => _state.HasBrush);
            FinishCommand = new RelayCommand(() => RequestFinish());
            DiscardCommand = new RelayCommand(RequestDiscard, () => _state.HasPendingEdits);
        }

        public void Update(CsgStateSnapshot state)
        {
            _state = state;

            if (_keepBrushAfterApply != state.KeepBrushAfterApply)
            {
                _keepBrushAfterApply = state.KeepBrushAfterApply;

                OnPropertyChanged(nameof(KeepBrushAfterApply));
            }

            OnPropertyChanged(nameof(TargetName));
            OnPropertyChanged(nameof(StatusText));
            OnPropertyChanged(nameof(HasStatusText));
            OnPropertyChanged(nameof(HasBrush));
            OnPropertyChanged(nameof(IsBrushSelected));
            OnPropertyChanged(nameof(IsPlacing));
            OnPropertyChanged(nameof(IsBoxShape));
            OnPropertyChanged(nameof(IsSphereShape));
            OnPropertyChanged(nameof(IsCylinderShape));
            OnPropertyChanged(nameof(IsUnion));
            OnPropertyChanged(nameof(IsSubtract));
            OnPropertyChanged(nameof(IsIntersect));
            OnPropertyChanged(nameof(OperationDescription));
            OnPropertyChanged(nameof(ApplyText));
            OnPropertyChanged(nameof(HasPendingEdits));
            OnPropertyChanged(nameof(ShowSizing));
            OnPropertyChanged(nameof(IsSizingBox));
            OnPropertyChanged(nameof(IsSizingSphere));
            OnPropertyChanged(nameof(IsSizingCylinder));
            OnPropertyChanged(nameof(IsSizingRound));
            OnPropertyChanged(nameof(BoxSizeX));
            OnPropertyChanged(nameof(BoxSizeY));
            OnPropertyChanged(nameof(BoxSizeZ));
            OnPropertyChanged(nameof(Radius));
            OnPropertyChanged(nameof(Height));

            (ApplyCommand as RelayCommand)?.RaiseCanExecuteChanged();
            (CancelPlacementCommand as RelayCommand)?.RaiseCanExecuteChanged();
            (SelectBrushCommand as RelayCommand)?.RaiseCanExecuteChanged();
            (ResetBrushCommand as RelayCommand)?.RaiseCanExecuteChanged();
            (RemoveBrushCommand as RelayCommand)?.RaiseCanExecuteChanged();
            (DiscardCommand as RelayCommand)?.RaiseCanExecuteChanged();
        }

        public void RequestFinish(Action? onCancelled = null)
        {
            if (!_state.HasBrush)
            {
                PostToCsgState(csgState => csgState.Exit(/* saveEdits */ true));

                return;
            }

            MessageBox message = MessageBox.Warning("Unapplied Brush", "The brush hasn't been applied to the mesh yet. Are you sure you want to finish without applying?");

            if (_state.CanApply)
            {
                message.Button("Apply and Finish", () => PostToCsgState(csgState =>
                {
                    csgState.ApplyBrush();

                    if (string.IsNullOrEmpty(csgState.GetStatusText()))
                    {
                        csgState.Exit(/* saveEdits */ true);
                    }
                }));
            }

            message
                .Button("Finish Without Applying", () => PostToCsgState(csgState => csgState.Exit(/* saveEdits */ true)))
                .Button("Cancel", () =>
                {
                    if (onCancelled != null)
                    {
                        Dispatcher.UIThread.Post(onCancelled);
                    }
                })
                .Show();
        }

        public void RequestDiscard()
        {
            if (!_state.HasPendingEdits)
            {
                PostToCsgState(csgState => csgState.Exit(/* saveEdits */ false));

                return;
            }

            MessageBox.Warning("Discard Edits?", "Are you sure you want to discard edits?")
                .Button("Discard", () => PostToCsgState(csgState => csgState.Exit(/* saveEdits */ false)))
                .Button("Cancel", () => { })
                .Show();
        }

        private static float RoundForDisplay(float value) => MathF.Round(value, 4);

        private void PushSizingHalfExtents(Vec3f halfExtents)
        {
            if (halfExtents.X <= 0.0f || halfExtents.Y <= 0.0f || halfExtents.Z <= 0.0f)
            {
                return;
            }

            Vec3f current = _state.SizingHalfExtents;

            if (MathF.Abs(halfExtents.X - current.X) < 0.00001f
                && MathF.Abs(halfExtents.Y - current.Y) < 0.00001f
                && MathF.Abs(halfExtents.Z - current.Z) < 0.00001f)
            {
                return;
            }

            _state.SizingHalfExtents = halfExtents;

            PostToCsgState(csgState => csgState.SetSizingHalfExtents(halfExtents));
        }

        private void PostToCsgState(Action<EditorCsgState> action)
        {
            _ = EngineManager.PostToSimThread(() =>
            {
                EditorCsgState? csgState = _editorSubsystem.EditorCsgState;

                if (csgState != null)
                {
                    action(csgState);
                }

                _refreshState();
            });
        }
    }
}
