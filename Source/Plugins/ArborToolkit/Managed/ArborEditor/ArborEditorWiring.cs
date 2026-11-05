using System;
using System.Collections.Generic;
using System.Globalization;
using System.Linq;
using Avalonia.Threading;
using Hyperion;
using Hyperion.Editor.Commands;
using Hyperion.Editor.Services;
using Hyperion.Editor.Views.Panels;

namespace Hyperion.Editor.ViewModels
{
    [EditorPlugin("ArborToolkit")]
    public sealed class ArborEditorWiring : IEditorPlugin
    {
        private readonly ArborPanelHost _host = new ArborPanelHost();

        public void Register(EditorPluginRegistry registry)
        {
            registry.RegisterView<ArborPanelViewModel, ArborPanelView>();

            registry.AddAddSubmenu("Arbor", "Tree, Shrub or Vine", () => _host.GetPresetMenuItems(ArborPlantKind.Tree));
            registry.AddAddSubmenu("Arbor", "Ground Cover", () => _host.GetPresetMenuItems(ArborPlantKind.Cover));
            registry.AddAddSubmenu("Arbor", "Rock", () => _host.GetPresetMenuItems(ArborPlantKind.Rock));

            registry.OnSimulationChanged(isSimulating => _host.Refresh());

            _ = EngineManager.PostToSimThread(_host.Bind);
        }
    }

    public sealed class ArborPanelHost
    {
        private ArborPanelViewModel? _panel;

        private DelegateHandler? _stateChangedHandler;

        // the built-in presets of each kind, read once on the sim thread for the Add menus
        private string[][] _presets = { Array.Empty<string>(), Array.Empty<string>(), Array.Empty<string>() };

        internal void Bind()
        {
            EditorArborState? arborState = GetEditorSubsystem()?.GetArborState();

            if (arborState == null)
            {
                return;
            }

            ReadPresets(arborState);

            WeakReference<ArborPanelHost> weakThis = new(this);

            _stateChangedHandler?.Remove();
            _stateChangedHandler = arborState.GetOnStateChangedDelegate()
                .Bind(() =>
                {
                    if (weakThis.TryGetTarget(out ArborPanelHost? target))
                    {
                        target.Refresh();
                    }
                });

            Refresh();
        }

        private void ReadPresets(EditorArborState arborState)
        {
            if (!arborState.IsAvailable() || _presets.Any(presets => presets.Length > 0))
            {
                return;
            }

            _presets = Enum.GetValues<ArborPlantKind>()
                .Select(kind => Enumerable.Range(0, (int)arborState.GetPresetCount((uint)kind))
                    .Select(index => arborState.GetPresetName((uint)kind, (uint)index) ?? string.Empty)
                    .ToArray())
                .ToArray();
        }

        internal IReadOnlyList<EditorPluginMenuItem> GetPresetMenuItems(ArborPlantKind kind)
        {
            return _presets[(int)kind]
                .Select(preset => new EditorPluginMenuItem(DisplayName(preset), new RelayCommand(() => CreatePlant(kind, preset))))
                .ToList();
        }

        // the plant is grown in Arbor's own edit mode, and becomes a prefab asset when it is baked
        internal void CreatePlant(ArborPlantKind kind, string preset)
        {
            MainWindowViewModel.Instance?.RequestEnterEditMode("Arbor", () => GetEditorSubsystem()?.GetArborState()?.EnterNewPlant((uint)kind, preset) ?? false);
        }

        // douglas_fir -> Douglas Fir
        private static string DisplayName(string preset)
        {
            return CultureInfo.InvariantCulture.TextInfo.ToTitleCase(preset.Replace('_', ' '));
        }

        internal void Refresh()
        {
            if (!EngineManager.IsOnSimThread)
            {
                _ = EngineManager.PostToSimThread(Refresh);

                return;
            }

            EditorSubsystem? editorSubsystem = GetEditorSubsystem();
            EditorArborState? arborState = editorSubsystem?.GetArborState();

            if (arborState != null)
            {
                ReadPresets(arborState);
            }

            ArborSnapshot snapshot = arborState != null && !editorSubsystem!.IsSimulating()
                ? ArborSnapshot.Read(arborState, _panel?.LayoutVersion)
                : new ArborSnapshot();

            Dispatcher.UIThread.Post(() => Apply(snapshot));
        }

        private static EditorSubsystem? GetEditorSubsystem()
        {
            return EngineManager.EditorGame?.EditorSubsystem;
        }

        private void Apply(ArborSnapshot snapshot)
        {
            if (snapshot.HasTarget)
            {
                if (_panel == null)
                {
                    _panel = new ArborPanelViewModel(PostToArborState, ClosePanel);

                    PanelService.Instance.OpenPanel(_panel);
                }

                _panel.Update(snapshot);
            }
            else if (_panel != null)
            {
                ArborPanelViewModel panel = _panel;
                _panel = null;

                PanelService.Instance.RemovePanel(panel);
            }
        }

        private void PostToArborState(Action<EditorArborState> action)
        {
            _ = EngineManager.PostToSimThread(() =>
            {
                EditorArborState? arborState = GetEditorSubsystem()?.GetArborState();

                if (arborState != null)
                {
                    action(arborState);
                }
            });
        }

        // closing the panel leaves the mode, asking first about a plant not yet baked
        private void ClosePanel()
        {
            ArborPanelViewModel? panel = _panel;

            MainWindowViewModel.Instance?.RequestExitEditMode(afterExit: null, onCancelled: () =>
            {
                if (panel != null)
                {
                    Dispatcher.UIThread.Post(() => PanelService.Instance.OpenPanel(panel));
                }
            });
        }
    }
}
