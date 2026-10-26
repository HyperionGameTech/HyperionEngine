using System.Linq;
using Hyperion.Editor;
using Hyperion.Editor.Commands;
using Hyperion.Editor.Services;
using Hyperion.Editor.ViewModels;

namespace Hyperion.PluginTest.Editor
{
    [EditorPlugin("PluginTest")]
    public sealed class PluginTestEditorPlugin : IEditorPlugin
    {
        public void Register(EditorPluginRegistry registry)
        {
            registry.RegisterView<PluginTestPanelViewModel, PluginTestPanelView>();
            registry.AddToolMenuItem("Plugin Test...", new RelayCommand(() => PanelService.Instance.OpenPanel(new PluginTestPanelViewModel())));

            // the plugin's runtime C# and generated bindings, called from its editor C#
            Logger.Log(LogLevel.Info, "PluginTestEditor: PluginTestSystem total ticks from C#: {0}", PluginTestPanelViewModel.GetTotalTicks()?.ToString() ?? "no system");
        }
    }

    public sealed class PluginTestPanelViewModel : EditorPanelViewModel
    {
        public string EnabledPlugins { get; }
        public string TotalTicks { get; }

        public PluginTestPanelViewModel()
            : base("Plugin Test")
        {
            EnabledPlugins = EngineManager.CurrentProject != null
                ? string.Join(", ", EngineManager.CurrentProject.GetEnabledPlugins().Cast<Name>().Select(pluginName => pluginName.ToString()))
                : string.Empty;

            TotalTicks = GetTotalTicks()?.ToString() ?? "no system";
        }

        public static uint? GetTotalTicks()
        {
            return EngineManager.CurrentProject?.World?.GetSystem<PluginTestSystem>()?.GetTotalTicks();
        }
    }
}
