/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#include <Plugins/PluginModule.hpp>
#include <Plugins/PluginContext.hpp>

#include <ActiveRagdoll/ActiveRagdollSystem.hpp>

namespace Hyperion {

class ActiveRagdollModule final : public IPluginModule
{
public:
    void OnLoad(PluginContext& context) override
    {
        context.RegisterWorldSystem<ActiveRagdollSystem>();
    }
};

} // namespace Hyperion

HYP_DEFINE_PLUGIN_MODULE(ActiveRagdoll, ActiveRagdoll, ::Hyperion::ActiveRagdollModule)
