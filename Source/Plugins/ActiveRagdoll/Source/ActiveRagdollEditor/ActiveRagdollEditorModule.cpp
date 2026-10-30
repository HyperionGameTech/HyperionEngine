/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#include <Plugins/PluginModule.hpp>
#include <Plugins/PluginContext.hpp>

namespace Hyperion {

class ActiveRagdollEditorModule final : public IPluginModule
{
public:
    void OnLoad(PluginContext& context) override
    {
    }
};

} // namespace Hyperion

HYP_DEFINE_PLUGIN_MODULE(ActiveRagdoll, ActiveRagdollEditor, ::Hyperion::ActiveRagdollEditorModule)
