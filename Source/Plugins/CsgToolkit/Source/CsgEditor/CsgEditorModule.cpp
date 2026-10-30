/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#include <Plugins/PluginModule.hpp>
#include <Plugins/PluginContext.hpp>

#include <CsgEditor/CsgEditorExtension.hpp>

namespace Hyperion {

class CsgEditorModule final : public IPluginModule
{
public:
    void OnLoad(PluginContext& context) override
    {
        RegisterEditorExtension<CsgEditorExtension>(context);
    }
};

} // namespace Hyperion

HYP_DEFINE_PLUGIN_MODULE(CsgToolkit, CsgEditor, ::Hyperion::CsgEditorModule)
