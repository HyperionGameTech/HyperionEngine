/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#include <ScenePch.hpp>

#include <Scene/Systems/BoneAttachmentSystem.hpp>

#include <Scene/Scene.hpp>
#include <Scene/Entity.hpp>
#include <Scene/EntityManager.hpp>

#include <Scene/Animation/Skeleton.hpp>

#include <BoneAttachmentSystem.generated.inl>

namespace Hyperion {

namespace {

const Entity* FindSkinnedAncestor(const Entity& entity)
{
    for (const Node* node = entity.GetParent(); node != nullptr; node = node->GetParent())
    {
        const Entity* ancestor = TryCast<Entity>(node);

        if (!ancestor || !ancestor->GetEntityManager())
        {
            continue;
        }

        if (const MeshComponent* meshComponent = ancestor->TryGetComponent<MeshComponent>(); meshComponent && meshComponent->skeleton.IsValid())
        {
            return ancestor;
        }
    }

    return nullptr;
}

} // namespace

bool BoneAttachmentSystem::ShouldProcessScene(Scene* scene) const
{
    return !(scene->GetSceneFlags() & (SceneFlags::UI | SceneFlags::DETACHED));
}

void BoneAttachmentSystem::Process(float delta, Span<Handle<Scene>> scenes)
{
    HYP_SCOPE;

    for (Scene* scene : scenes)
    {
        if (!ShouldProcessScene(scene))
        {
            continue;
        }

        for (auto [entity, attachmentComponent] : scene->GetEntityManager()->GetEntitySet<BoneAttachmentComponent>().GetScopedView(GetComponentInfos()))
        {
            if (!attachmentComponent.socket.IsValid() || !entity->GetParent())
            {
                continue;
            }

            const Entity* target = FindSkinnedAncestor(*entity);

            if (!target)
            {
                continue;
            }

            const MeshComponent& meshComponent = target->GetComponent<MeshComponent>();
            const Skeleton& skeleton = *meshComponent.skeleton;
            const SkeletonBindData& bindData = skeleton.GetBindData();

            Name boneName = attachmentComponent.socket;
            Transform socketOffset;

            if (const SkeletonSocket* socket = skeleton.FindSocket(attachmentComponent.socket))
            {
                boneName = socket->boneName;
                socketOffset = socket->offset;
            }

            const uint32 slot = bindData.FindSlot(boneName);

            if (slot == SkeletonBindData::InvalidSlot)
            {
                continue;
            }

            const Handle<Skeleton>& renderSkeleton = meshComponent.GetRenderSkeleton();
            const Span<const Mat4f> poseMatrices = renderSkeleton.IsValid() ? renderSkeleton->GetPoseModelMatrices() : Span<const Mat4f>();

            const Mat4f boneMatrix = poseMatrices.Size() == bindData.NumSlots()
                ? poseMatrices[slot]
                : bindData.inverseBindMatrices[slot].Inverse();

            const Mat4f localMatrix = entity->GetParent()->GetWorldMatrix().Inverse()
                * target->GetWorldMatrix()
                * boneMatrix
                * socketOffset.GetMatrix()
                * attachmentComponent.offset.GetMatrix();

            const Transform localTransform(localMatrix.ExtractTranslation(), localMatrix.ExtractScale(), localMatrix.ExtractRotation());

            if (!(entity->GetLocalTransform() == localTransform))
            {
                entity->SetLocalTransform(localTransform);
            }
        }
    }
}

} // namespace Hyperion
