/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#include <Plugins/PluginModule.hpp>
#include <Plugins/PluginContext.hpp>

#include <ArborEditor/ArborEditorExtension.hpp>

namespace Hyperion {

EDITOR_API HYP_DECLARE_LOG_CHANNEL(Editor);
ARBOREDITOR_API HYP_DEFINE_LOG_SUBCHANNEL(Arbor, Editor);

class ArborEditorModule final : public IPluginModule
{
public:
    void OnLoad(PluginContext& context) override
    {
        RegisterEditorExtension<ArborEditorExtension>(context);
    }
};

} // namespace Hyperion

HYP_DEFINE_PLUGIN_MODULE(ArborToolkit, ArborEditor, ::Hyperion::ArborEditorModule)
