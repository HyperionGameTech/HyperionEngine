using System;
using System.Windows.Input;
using Avalonia.Threading;
using Hyperion;
using Hyperion.Editor.Commands;
using Hyperion.Editor.Services;
using Hyperion.Editor.Views.Panels;

namespace Hyperion.Editor.ViewModels
{
    [EditorPlugin("CsgToolkit")]
    public sealed class CsgEditorWiring : IEditorPlugin
    {
        private readonly BrushToolsViewModel _brushTools = new BrushToolsViewModel();

        public void Register(EditorPluginRegistry registry)
        {
            registry.RegisterView<CsgPanelViewModel, CsgPanelView>();

            registry.AddMeshTool(_brushTools);

            registry.OnSimulationChanged(isSimulating => _brushTools.Refresh());

            _ = EngineManager.PostToSimThread(_brushTools.Bind);
        }
    }

    public sealed class BrushToolsViewModel : EditorMeshToolViewModel
    {
        private CsgStateSnapshot _state = new CsgStateSnapshot();

        private CsgPanelViewModel? _panel;

        private DelegateHandler? _stateChangedHandler;

        public override string Header => "Brush Tools";

        public override ICommand ToggleCommand { get; }

        public BrushToolsViewModel()
        {
            ToggleCommand = new RelayCommand(Toggle);
        }

        internal void Bind()
        {
            EditorCsgState? csgState = GetEditorSubsystem()?.GetCsgState();

            if (csgState == null)
            {
                return;
            }

            WeakReference<BrushToolsViewModel> weakThis = new WeakReference<BrushToolsViewModel>(this);

            _stateChangedHandler?.Remove();
            _stateChangedHandler = csgState.GetOnStateChangedDelegate()
                .Bind(() =>
                {
                    if (!weakThis.TryGetTarget(out BrushToolsViewModel? target))
                    {
                        return;
                    }

                    target.Refresh();
                });

            Refresh();
        }

        internal void Refresh()
        {
            if (!EngineManager.IsOnSimThread)
            {
                _ = EngineManager.PostToSimThread(Refresh);

                return;
            }

            EditorSubsystem? editorSubsystem = GetEditorSubsystem();

            if (editorSubsystem == null)
            {
                return;
            }

            MainWindowViewModel.Instance?.RefreshVolumeEditAvailability();

            CsgStateSnapshot snapshot = new();

            try
            {
                EditorCsgState? csgState = editorSubsystem.GetCsgState();

                snapshot.Simulating = editorSubsystem.IsSimulating();

                if (csgState != null)
                {
                    snapshot.Enabled = csgState.IsEnabled();
                    snapshot.CanEnable = csgState.CanEnter();
                    snapshot.HasBrush = csgState.HasBrush();
                    snapshot.IsBrushSelected = csgState.IsBrushSelected();
                    snapshot.IsPlacing = csgState.IsPlacing();
                    snapshot.PlacementShape = csgState.GetPlacementShape();
                    snapshot.CanApply = csgState.CanApply();
                    snapshot.HasPendingEdits = csgState.HasPendingEdits();
                    snapshot.KeepBrushAfterApply = csgState.IsKeepBrushAfterApply();
                    snapshot.BrushShape = csgState.GetBrushShape();
                    snapshot.Operation = csgState.GetOperation();
                    snapshot.TargetName = csgState.GetTargetNode()?.Name.ToString() ?? string.Empty;
                    snapshot.StatusText = csgState.GetStatusText() ?? string.Empty;
                    snapshot.SizingShape = csgState.GetSizingShape();
                    snapshot.SizingHalfExtents = csgState.GetSizingHalfExtents();
                }
            }
            catch (Exception ex)
            {
                Logger.Log(LogLevel.Warning, $"Failed to read CSG state from engine: {ex.Message}");

                return;
            }

            Dispatcher.UIThread.Post(() => Apply(snapshot));
        }

        private static EditorSubsystem? GetEditorSubsystem()
        {
            return EngineManager.EditorGame?.EditorSubsystem;
        }

        private void Apply(CsgStateSnapshot snapshot)
        {
            _state = snapshot;

            IsActive = snapshot.Enabled;
            CanEnable = snapshot.CanEnable;
            Tooltip = GetTooltip(snapshot);

            UpdatePanel();
        }

        private static string GetTooltip(CsgStateSnapshot state)
        {
            if (state.Enabled)
            {
                return "Leave CSG mode, keeping changes (Esc)";
            }

            if (state.Simulating)
            {
                return "Cannot use CSG while in simulation";
            }

            if (!state.CanEnable)
            {
                return "Select a mesh made of position, normal and UV channels";
            }

            return "Carve, grow or trim the selected mesh with box, sphere and cylinder brushes";
        }

        private void Toggle()
        {
            if (_panel != null && _state.Enabled && _state.HasBrush)
            {
                _panel.RequestFinish();

                return;
            }

            _ = EngineManager.PostToSimThread(() =>
            {
                EditorCsgState? csgState = GetEditorSubsystem()?.GetCsgState();

                if (csgState == null)
                {
                    return;
                }

                if (csgState.IsEnabled())
                {
                    csgState.Exit(/* saveEdits */ true);
                }
                else
                {
                    csgState.Enter();
                }

                // entering CSG turns mesh edit and the terrain/decal tools off
                Refresh();
                MainWindowViewModel.Instance?.RefreshToolStates();
            });
        }

        private void UpdatePanel()
        {
            EditorSubsystem? editorSubsystem = GetEditorSubsystem();

            if (_state.Enabled && editorSubsystem != null)
            {
                if (_panel == null)
                {
                    _panel = new CsgPanelViewModel(editorSubsystem, Refresh, ClosePanel);

                    PanelService.Instance.OpenPanel(_panel);
                }

                _panel.Update(_state);
            }
            else if (_panel != null)
            {
                CsgPanelViewModel panel = _panel;
                _panel = null;

                PanelService.Instance.RemovePanel(panel);
            }
        }

        private void ClosePanel()
        {
            CsgPanelViewModel? panel = _panel;

            if (panel != null && _state.Enabled && _state.HasBrush)
            {
                panel.RequestFinish(onCancelled: () => PanelService.Instance.OpenPanel(panel));

                return;
            }

            _ = EngineManager.PostToSimThread(() =>
            {
                EditorCsgState? csgState = GetEditorSubsystem()?.GetCsgState();

                if (csgState != null && csgState.IsEnabled())
                {
                    csgState.Exit(/* saveEdits */ true);
                }

                Refresh();
            });
        }
    }
}
