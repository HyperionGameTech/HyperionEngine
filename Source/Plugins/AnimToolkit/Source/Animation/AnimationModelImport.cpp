/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#include <HyperionPch.hpp>

#include <Animation/AnimationModelImport.hpp>
#include <Scene/Animation/Animation.hpp>
#include <Scene/Animation/Skeleton.hpp>
#include <Animation/HumanoidMapping.hpp>

#include <Scene/Prefab.hpp>
#include <Scene/Entity.hpp>
#include <Scene/EntityManager.hpp>

#include <Scene/Components/MeshComponent.hpp>
#include <Animation/AnimationComponent.hpp>

#include <Asset/ModelImport.hpp>

#include <AnimationModelImport.generated.inl>

namespace Hyperion {

namespace {

template <class Function>
void ForEachAnimatableEntity(Node& root, Function&& function)
{
    const auto visit = [&function](Node* node)
    {
        Entity* entity = TryCast<Entity>(node);

        if (!entity || !entity->GetEntityManager())
        {
            return;
        }

        if (const MeshComponent* meshComponent = entity->TryGetComponent<MeshComponent>();
                meshComponent && meshComponent->skeleton.IsValid())
        {
            function(*entity);
        }
    };

    visit(&root);

    for (Node* descendant : root.GetDescendants())
    {
        visit(descendant);
    }
}

} // namespace

const Class* AnimationModelImportProcessor::GetSettingsClass() const
{
    return AnimationModelImportSettings::StaticClass();
}

void AnimationModelImportProcessor::Process(ModelImportContents& contents, const AssetImportInfo& importInfo, const ModelImportProcessorSettings* settings)
{
    if (importInfo.settings.rigMode == ImportRigMode::Create && importInfo.settings.autoMapHumanoid && contents.skeleton.IsValid())
    {
        HumanoidSkeletonData::AutoMapSkeleton(*contents.skeleton);
    }

    if (!contents.prefab.IsValid() || !contents.prefab->GetRoot().IsValid())
    {
        return;
    }

    if (!static_cast<const AnimationModelImportSettings*>(settings)->addAnimationComponent)
    {
        return;
    }

    const Handle<Animation>& firstClip = contents.animations.Any() ? contents.animations[0] : Handle<Animation>::Null();

    ForEachAnimatableEntity(*contents.prefab->GetRoot(), [&firstClip](Entity& entity)
        {
            if (AnimationComponent* animationComponent = entity.TryGetComponent<AnimationComponent>())
            {
                if (!animationComponent->animation.IsValid())
                {
                    animationComponent->animation = firstClip;
                }

                return;
            }

            AnimationComponent animationComponent {};
            animationComponent.animation = firstClip;
            animationComponent.playbackState = {
                .animationIndex = 0,
                .status = AnimationPlaybackStatus::Playing,
                .loopMode = AnimationLoopMode::Repeat,
                .speed = 1.0f,
                .currentTime = 0.0f
            };

            entity.AddComponent<AnimationComponent>(animationComponent);
        });
}

void AnimationModelImportProcessor::OnClipsMerged(
    Node& root,
    const Map<const Animation*, Handle<Animation>>& mergedClipsByFreshClip)
{
    ForEachAnimatableEntity(root, [&mergedClipsByFreshClip](Entity& entity)
        {
            AnimationComponent* animationComponent = entity.TryGetComponent<AnimationComponent>();

            if (!animationComponent || !animationComponent->animation.IsValid())
            {
                return;
            }

            if (auto mergedIt = mergedClipsByFreshClip.Find(animationComponent->animation.Get()); mergedIt != mergedClipsByFreshClip.End())
            {
                animationComponent->animation = mergedIt->second;
            }
        });
}

} // namespace Hyperion
