/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#include <Plugins/PluginModule.hpp>
#include <Plugins/PluginContext.hpp>

#include <AnimationEditor/AnimationEditorExtension.hpp>
#include <AnimationEditor/AnimationAssetDrop.hpp>
#include <AnimationEditor/AnimationPlayerSetup.hpp>

namespace Hyperion {

class AnimationEditorModule final : public IPluginModule
{
public:
    void OnLoad(PluginContext& context) override
    {
        RegisterEditorExtension<AnimationEditorExtension>(context);

        RegisterEntityAssetDropHandler<AnimationAssetDropHandler>(context);
        RegisterEntityAssetDropHandler<AnimationGraphAssetDropHandler>(context);

        RegisterPlayerSetupProcessor<AnimationPlayerSetupProcessor>(context);
    }
};

} // namespace Hyperion

HYP_DEFINE_PLUGIN_MODULE(AnimToolkit, AnimationEditor, ::Hyperion::AnimationEditorModule)
