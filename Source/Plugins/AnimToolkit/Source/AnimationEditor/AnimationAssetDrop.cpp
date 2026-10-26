/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#include <EditorPch.hpp>

#include <AnimationEditor/AnimationAssetDrop.hpp>
#include <Editor/EditorAction.hpp>
#include <Editor/EditorSubsystem.hpp>

#include <Scene/Entity.hpp>
#include <Scene/EntityManager.hpp>

#include <Scene/Components/MeshComponent.hpp>
#include <Animation/AnimationComponent.hpp>
#include <Animation/CharacterModelComponent.hpp>

#include <Scene/Animation/Animation.hpp>
#include <Animation/AnimationGraph.hpp>

#include <Core/Utilities/Format.hpp>

namespace Hyperion {

bool AnimationAssetDropHandler::AcceptsAsset(const AssetObject* asset) const
{
    return TryCast<Animation>(asset) != nullptr;
}

bool AnimationAssetDropHandler::CanApply(const AssetObject* asset, const Entity* entity) const
{
    const MeshComponent* meshComponent = entity->TryGetComponent<MeshComponent>();

    return meshComponent != nullptr && meshComponent->skeleton.IsValid();
}

Handle<EditorActionBase> AnimationAssetDropHandler::CreateAction(
    EditorSubsystem* subsystem,
    const Handle<AssetObject>& asset,
    const Handle<Entity>& entity) const
{
    Handle<Animation> animation = TryCast<Animation>(asset);

    const AnimationComponent* pPreviousAnimationComponent = entity->TryGetComponent<AnimationComponent>();

    if (pPreviousAnimationComponent && pPreviousAnimationComponent->animation == animation)
    {
        return nullptr;
    }

    AnimationComponent animationComponent = pPreviousAnimationComponent
        ? *pPreviousAnimationComponent
        : AnimationComponent {};

    return MakeEntityDropAction(
        subsystem,
        HYP_FORMAT("Assign Animation {}", animation->GetName()),
        entity,
        [entity, animation](EditorSubsystem*)
        {
            if (!entity->HasComponent<AnimationComponent>())
            {
                entity->AddComponent<AnimationComponent>(AnimationComponent {});
            }

            entity->GetComponent<AnimationComponent>().Play(animation);
            entity->MarkDirty();
        },
        [entity,
            hadAnimationComponent = pPreviousAnimationComponent != nullptr,
            previousAnimationComponent = std::move(animationComponent)](EditorSubsystem*)
        {
            if (!hadAnimationComponent)
            {
                entity->RemoveComponent<AnimationComponent>();
            }
            else if (AnimationComponent* currAnimationComponent = entity->TryGetComponent<AnimationComponent>())
            {
                *currAnimationComponent = previousAnimationComponent;
            }

            entity->MarkDirty();
        });
}

bool AnimationGraphAssetDropHandler::AcceptsAsset(const AssetObject* asset) const
{
    return TryCast<AnimationGraph>(asset) != nullptr;
}

bool AnimationGraphAssetDropHandler::CanApply(const AssetObject* asset, const Entity* entity) const
{
    return entity->HasComponent<CharacterModelComponent>()
        || AnimationAssetDropHandler().CanApply(asset, entity);
}

Handle<EditorActionBase> AnimationGraphAssetDropHandler::CreateAction(EditorSubsystem* subsystem, const Handle<AssetObject>& asset, const Handle<Entity>& entity) const
{
    Handle<AnimationGraph> graph = TryCast<AnimationGraph>(asset);

    if (const CharacterModelComponent* characterModel = entity->TryGetComponent<CharacterModelComponent>())
    {
        const Handle<AnimationGraph> previousGraph = characterModel->graph;

        if (previousGraph == graph)
        {
            return nullptr;
        }

        return MakeEntityDropAction(
            subsystem,
            HYP_FORMAT("Assign Character Graph {}", graph->GetName()),
            entity,
            [entity, graph](EditorSubsystem*)
            {
                entity->GetComponent<CharacterModelComponent>().graph = graph;
                entity->MarkDirty();
            },
            [entity, previousGraph](EditorSubsystem*)
            {
                if (CharacterModelComponent* characterModelComponent = entity->TryGetComponent<CharacterModelComponent>())
                {
                    characterModelComponent->graph = previousGraph;
                }

                entity->MarkDirty();
            });
    }

    const AnimationComponent* pPreviousAnimationComponent = entity->TryGetComponent<AnimationComponent>();
    const Handle<AnimationGraph> previousGraph = pPreviousAnimationComponent ? pPreviousAnimationComponent->graph : Handle<AnimationGraph>::Null();

    if (pPreviousAnimationComponent && previousGraph == graph)
    {
        return nullptr;
    }

    return MakeEntityDropAction(
        subsystem,
        HYP_FORMAT("Assign Animation Graph {}", graph->GetName()),
        entity,
        [entity, graph](EditorSubsystem*)
        {
            if (!entity->HasComponent<AnimationComponent>())
            {
                entity->AddComponent<AnimationComponent>(AnimationComponent {});
            }

            entity->GetComponent<AnimationComponent>().graph = graph;
            entity->MarkDirty();
        },
        [entity,
            hadAnimationComponent = (pPreviousAnimationComponent != nullptr),
            previousGraph = std::move(previousGraph)](EditorSubsystem*)
        {
            if (!hadAnimationComponent)
            {
                entity->RemoveComponent<AnimationComponent>();
            }
            else if (AnimationComponent* animationComponent = entity->TryGetComponent<AnimationComponent>())
            {
                animationComponent->graph = previousGraph;
            }

            entity->MarkDirty();
        });
}


} // namespace Hyperion
