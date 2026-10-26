/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#include <Editor/Commands/EditorCommandsCommon.hpp>

#include <Plugins/PluginModule.hpp>
#include <Plugins/PluginContext.hpp>
#include <Plugins/PluginManager.hpp>

#include <Scene/World.hpp>

#include <PluginTest/PluginTestSystem.hpp>

namespace Hyperion {

class PluginTestEditorModule final : public IPluginModule
{
public:
    void OnLoad(PluginContext& context) override
    {
        HYP_LOG(Editor, Info, "PluginTestEditor loaded (editor host: {})", context.IsEditorHost());
    }
};

// logs the enabled plugins, and whether the project's World has the PluginTest system
class EditorCommandPluginTestStatus final : public EditorCommandBase
{
    HYP_OBJECT_BODY(EditorCommandPluginTestStatus);

public:
    virtual ~EditorCommandPluginTestStatus() override = default;

    virtual String GetText() const override
    {
        return "Plugin Test Status";
    }

    virtual void Execute(EditorSubsystem* subsystem) override
    {
        AssertOnThread(g_simThread);

        String enabledPlugins;

        for (Name pluginName : PluginManager::GetInstance().GetEnabledPlugins())
        {
            enabledPlugins += String(pluginName.LookupString()) + " ";
        }

        const Handle<EditorProject>& project = subsystem->GetCurrentProject();
        const World* world = project.IsValid() ? project->GetWorld().Get() : nullptr;

        HYP_LOG(Editor, Info, "PluginTestStatus: enabled plugins [ {}], world has PluginTestSystem: {}",
            enabledPlugins,
            world != nullptr && world->GetSystem<PluginTestSystem>() != nullptr);
    }
};

DEFINE_EDITOR_COMMAND(PluginTestStatus);

} // namespace Hyperion

HYP_DEFINE_PLUGIN_MODULE(PluginTest, PluginTestEditor, ::Hyperion::PluginTestEditorModule)
