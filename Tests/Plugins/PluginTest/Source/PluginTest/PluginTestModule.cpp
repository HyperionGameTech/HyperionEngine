/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#include <PluginTest/PluginTestSystem.hpp>

#include <Plugins/PluginModule.hpp>
#include <Plugins/PluginContext.hpp>

namespace Hyperion {

class PluginTestModule final : public IPluginModule
{
public:
    void OnLoad(PluginContext& context) override
    {
        context.RegisterWorldSystem<PluginTestSystem>();
    }
};

} // namespace Hyperion

HYP_DEFINE_PLUGIN_MODULE(PluginTest, PluginTest, ::Hyperion::PluginTestModule)
