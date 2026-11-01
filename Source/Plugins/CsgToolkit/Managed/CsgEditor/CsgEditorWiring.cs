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
        private CsgStateSnapshot _state = new();

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

            snapshot.Simulating = Read("IsSimulating", () => editorSubsystem.IsSimulating(), false);

            EditorCsgState? csgState = Read("GetCsgState", () => editorSubsystem.GetCsgState(), null);

            if (csgState is EditorCsgState state)
            {
                snapshot.Enabled = Read("IsEnabled", () => state.IsEnabled(), false);
                snapshot.CanEnable = Read("CanEnter", () => state.CanEnter(), false);
                snapshot.HasBrush = Read("HasBrush", () => state.HasBrush(), false);
                snapshot.IsBrushSelected = Read("IsBrushSelected", () => state.IsBrushSelected(), false);
                snapshot.IsPlacing = Read("IsPlacing", () => state.IsPlacing(), false);
                snapshot.PlacementShape = Read("GetPlacementShape", () => state.GetPlacementShape(), CsgBrushShape.Box);
                snapshot.CanApply = Read("CanApply", () => state.CanApply(), false);
                snapshot.HasPendingEdits = Read("HasPendingEdits", () => state.HasPendingEdits(), false);
                snapshot.KeepBrushAfterApply = Read("IsKeepBrushAfterApply", () => state.IsKeepBrushAfterApply(), false);
                snapshot.AlignToSurface = Read("IsAlignToSurface", () => state.IsAlignToSurface(), false);
                snapshot.OutlinesShown = Read("AreOutlinesShown", () => state.AreOutlinesShown(), true);
                snapshot.BrushShape = Read("GetBrushShape", () => state.GetBrushShape(), CsgBrushShape.Box);
                snapshot.Operation = Read("GetOperation", () => state.GetOperation(), CsgOperation.Subtract);
                snapshot.Targets = Read("targets", () => ReadTargets(state), Array.Empty<CsgTargetSnapshot>());
                snapshot.TargetsPinned = Read("AreTargetsPinned", () => state.AreTargetsPinned(), false);
                snapshot.StatusText = Read("GetStatusText", () => state.GetStatusText() ?? string.Empty, string.Empty);
                snapshot.SizingShape = Read("GetSizingShape", () => state.GetSizingShape(), CsgBrushShape.Box);
                snapshot.SizingHalfExtents = Read("GetSizingHalfExtents", () => state.GetSizingHalfExtents(), new Vec3f(0.5f));
            }

            Dispatcher.UIThread.Post(() => Apply(snapshot));
        }

        private static T Read<T>(string name, Func<T> getter, T fallback)
        {
            try
            {
                return getter();
            }
            catch (Exception ex)
            {
                Logger.Log(LogLevel.Warning, $"Failed to read CSG state ({name}): {ex.Message}");

                return fallback;
            }
        }

        private static CsgTargetSnapshot[] ReadTargets(EditorCsgState csgState)
        {
            uint count = csgState.GetTargetCount();

            CsgTargetSnapshot[] targets = new CsgTargetSnapshot[count];

            for (uint index = 0; index < count; index++)
            {
                targets[index] = new CsgTargetSnapshot(
                    csgState.GetTargetName(index) ?? string.Empty,
                    csgState.GetTargetIssue(index) ?? string.Empty,
                    csgState.IsTargetIncluded(index));
            }

            return targets;
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
                return "Not available while editing a prefab";
            }

            return "Carve, grow or trim meshes with box, sphere and cylinder brushes";
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
