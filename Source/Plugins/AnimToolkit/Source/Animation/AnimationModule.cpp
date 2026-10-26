/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#include <Plugins/PluginModule.hpp>
#include <Plugins/PluginContext.hpp>

#include <Scene/Animation/Skeleton.hpp>
#include <Scene/Animation/Animation.hpp>

#include <Animation/AnimationPose.hpp>
#include <Animation/AnimationModelImport.hpp>
#include <Animation/AnimationSystem.hpp>
#include <Animation/BoneAttachmentSystem.hpp>
#include <Animation/CharacterModelSystem.hpp>

namespace Hyperion {

static uint32 CountRetargetedTracks(const Animation& animation, const Skeleton& skeleton)
{
    return PoseOps::CountBoundSlots(PoseOps::BindClip(animation, skeleton));
}

class AnimationModule final : public IPluginModule
{
public:
    void OnLoad(PluginContext& context) override
    {
        context.RegisterWorldSystem<CharacterModelSystem>();
        context.RegisterWorldSystem<AnimationSystem>();
        context.RegisterWorldSystem<BoneAttachmentSystem>();

        context.RegisterModelImportProcessor<AnimationModelImportProcessor>();

        Skeleton::SetBoundTrackCounter(&CountRetargetedTracks);

        context.AddCleanup([]()
            {
                Skeleton::SetBoundTrackCounter(nullptr);
            });
    }
};

} // namespace Hyperion

HYP_DEFINE_PLUGIN_MODULE(AnimToolkit, Animation, ::Hyperion::AnimationModule)
