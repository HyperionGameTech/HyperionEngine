/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <Scene/System.hpp>
#include <Scene/Components/AnimationComponent.hpp>
#include <Scene/Animation/AnimationPose.hpp>

#include <Core/Containers/Map.hpp>

#include <Core/Resource/Resource.hpp>
#include <Core/Resource/ResLock.hpp>

#include <Asset/AssetReference.hpp>

#include <Framework/EngineMemory.hpp>

namespace Hyperion {

class Skeleton;
class Animation;
class BoneMask;
struct SkeletonBindData;

struct MeshComponent;

struct AnimationInstanceState
{
    const Skeleton* source = nullptr;
    uint32 sourceRevision = 0;
    Handle<Skeleton> instance;

    // blended keyframes carried between frames; the twist and other per-frame offsets go on renderPose only
    Array<Transform> pose;
    Array<Transform> renderPose;

    Array<Mat4f> modelMatrices;
    Array<Mat4f> skinningMatrices;

    Map<const Animation*, ClipBinding> clipBindings;

    const BoneMask* layerMask = nullptr;
    Array<float> layerMaskWeights;

    float previewTime = 0.0f;
    float previewDelta = 0.0f;
    bool isPreviewing = false;
};

HYP_CLASS(NoScriptBindings, Serialize=false)
class AnimationSystem final : public SystemBase
{
    HYP_OBJECT_BODY(AnimationSystem);

public:
    ~AnimationSystem() override;

    bool ShouldProcessScene(Scene* scene) const override;

    void OnEntityAdded(Entity* entity) override;
    void OnEntityRemoved(Entity* entity) override;

    void Process(float delta, Span<Handle<Scene>> scenes) override;

    // edit-mode preview: poses the entity at `time` of its current clip while the game isn't simulating
    void SetPreviewTime(Entity* entity, float time);
    void StopPreview(Entity* entity);

    // runs an entity's graph outside simulation, advancing it by delta on the next update
    void AdvanceGraphPreview(Entity* entity, float delta);

    Span<const Transform> GetRenderPose(Entity* entity) const;

private:
    SystemComponentDescriptors GetComponentDescriptors() const override
    {
        return {
            ComponentDescriptor<AnimationComponent, ComponentAccess::READ_WRITE> {},
            ComponentDescriptor<MeshComponent, ComponentAccess::READ_WRITE> {}
        };
    }

    AnimationInstanceState& CreateInstanceState(Entity* entity, MeshComponent& meshComponent);
    void DestroyInstanceState(Entity* entity, MeshComponent* meshComponent);

    const ClipBinding& GetClipBinding(Entity* entity, AnimationInstanceState& state, const Animation& animation, const SkeletonBindData& bindData);

    void EvaluatePose(Entity* entity, const AnimationComponent& animationComponent, MeshComponent& meshComponent, const Animation& animation, float time, float blend);
    void EvaluateGraph(Entity* entity, AnimationComponent& animationComponent, MeshComponent& meshComponent, float delta);

    // keyed per entity since clones share a Skeleton
    Map<Entity*, Array<UniquePtr<TSharedResLock<AssetObject>>, SceneAllocator>, SceneAllocator> m_resourceHandles;

    Map<Entity*, AnimationInstanceState, SceneAllocator> m_instanceStates;

    uint32 m_numPreviewing = 0;
};

} // namespace Hyperion
