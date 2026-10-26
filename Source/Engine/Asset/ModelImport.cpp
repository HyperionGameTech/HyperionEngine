/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#include <AssetPch.hpp>

#include <Asset/ModelImport.hpp>
#include <Asset/AssetRegistry.hpp>
#include <Asset/Assets.hpp>

#include <Scene/Prefab.hpp>
#include <Scene/Entity.hpp>
#include <Scene/EntityManager.hpp>

#include <Scene/Components/MeshComponent.hpp>
#include <Scene/Components/AnimationComponent.hpp>

#include <Scene/Animation/Skeleton.hpp>
#include <Scene/Animation/Bone.hpp>
#include <Scene/Animation/Animation.hpp>

#include <Rendering/Mesh.hpp>
#include <Rendering/Material.hpp>
#include <Rendering/Util/MeshLodGenerator.hpp>

#include <Core/Containers/Map.hpp>

#include <Core/Math/MathUtil.hpp>

#include <Core/Logging/Logger.hpp>
#include <Core/Logging/LogChannels.hpp>

namespace Hyperion {

ENGINE_API HYP_DECLARE_LOG_CHANNEL(Assets);

namespace {

template <class Function>
void ForEachSkinnedEntity(Node& root, Function&& function)
{
    const auto visit = [&function](Node* node)
    {
        Entity* entity = TryCast<Entity>(node);

        if (!entity || !entity->GetEntityManager())
        {
            return;
        }

        if (MeshComponent* meshComponent = entity->TryGetComponent<MeshComponent>(); meshComponent && meshComponent->skeleton.IsValid())
        {
            function(*entity, *meshComponent);
        }
    };

    visit(&root);

    for (Node* descendant : root.GetDescendants())
    {
        visit(descendant);
    }
}

template <class Function>
void ForEachMeshEntity(Node& root, Function&& function)
{
    const auto visit = [&function](Node* node)
    {
        Entity* entity = TryCast<Entity>(node);

        if (!entity || !entity->GetEntityManager())
        {
            return;
        }

        if (MeshComponent* meshComponent = entity->TryGetComponent<MeshComponent>())
        {
            function(*entity, *meshComponent);
        }
    };

    visit(&root);

    for (Node* descendant : root.GetDescendants())
    {
        visit(descendant);
    }
}

const Handle<Animation>& FindClip(const Array<Handle<Animation>>& clips, Name name)
{
    for (const Handle<Animation>& clip : clips)
    {
        if (clip.IsValid() && clip->GetName() == name)
        {
            return clip;
        }
    }

    return Handle<Animation>::empty;
}

// the clip an earlier import of the same file made from the same take or split
const Handle<Animation>& FindPreviousImport(const Array<Handle<Animation>>& clips, const Animation& freshClip)
{
    const AssetImportInfo& freshImportInfo = freshClip.GetImportInfo();

    for (const Handle<Animation>& clip : clips)
    {
        if (!clip.IsValid())
        {
            continue;
        }

        const AssetImportInfo& importInfo = clip->GetImportInfo();

        if (importInfo.IsValid()
                ? importInfo.sourcePath == freshImportInfo.sourcePath && importInfo.sourceClip == freshImportInfo.sourceClip
                : clip->GetName() == freshClip.GetName())
        {
            return clip;
        }
    }

    return Handle<Animation>::empty;
}

} // namespace

ModelImportContents GatherModelImportContents(const Handle<Prefab>& prefab)
{
    ModelImportContents contents;
    contents.prefab = prefab;

    if (!prefab.IsValid() || !prefab->GetRoot().IsValid())
    {
        return contents;
    }

    ForEachSkinnedEntity(*prefab->GetRoot(), [&contents](Entity&, MeshComponent& meshComponent)
        {
            if (!contents.skeleton.IsValid())
            {
                contents.skeleton = meshComponent.skeleton;
            }
        });

    if (contents.skeleton.IsValid())
    {
        contents.animations = contents.skeleton->GetAnimations();
    }

    return contents;
}

Handle<Animation> MakeSubClip(const Animation& source, Name name, float startTime, float endTime)
{
    Handle<Animation> clip = MakeHandle<Animation>(name);

    endTime = MathUtil::Max(endTime, startTime);

    for (const Handle<AnimationTrack>& sourceTrack : source.GetTracks())
    {
        if (!sourceTrack.IsValid())
        {
            continue;
        }

        Array<Keyframe> keyframes;
        keyframes.PushBack(Keyframe(0.0f, sourceTrack->GetKeyframe(startTime).transform));

        for (const Keyframe& keyframe : sourceTrack->GetKeyframes())
        {
            if (keyframe.time > startTime && keyframe.time < endTime)
            {
                keyframes.PushBack(Keyframe(keyframe.time - startTime, keyframe.transform));
            }
        }

        if (endTime > startTime)
        {
            keyframes.PushBack(Keyframe(endTime - startTime, sourceTrack->GetKeyframe(endTime).transform));
        }

        Handle<AnimationTrack> track = MakeHandle<AnimationTrack>(NAME_FMT("{}_{}", name, sourceTrack->GetBoneName()), sourceTrack->GetBoneName());
        track->SetKeyframes(keyframes.ToSpan());

        clip->AddTrack(track);
    }

    return clip;
}

void ApplyModelImportSettings(ModelImportContents& contents, const AssetImportInfo& importInfo, const Handle<Skeleton>& targetSkeleton)
{
    const ModelImportSettings& settings = importInfo.settings;

    Array<Handle<Animation>> clips;

    for (const Handle<Animation>& take : contents.animations)
    {
        if (take.IsValid() && !settings.excludedTakes.Contains(take->GetName()))
        {
            clips.PushBack(take);
        }
    }

    const float frameRate = MathUtil::Max(settings.frameRate, 1.0f);

    for (const AnimationClipSplit& split : settings.splits)
    {
        const Handle<Animation>& take = FindClip(contents.animations, split.take);

        if (!take.IsValid() || !split.name.IsValid())
        {
            HYP_LOG(Assets, Warning, "Import of '{}': split '{}' names take '{}', which the file doesn't have", importInfo.sourcePath, split.name, split.take);

            continue;
        }

        Handle<Animation> clip = MakeSubClip(*take, split.name, float(split.startFrame) / frameRate, float(split.endFrame) / frameRate);
        clip->SetLooping(split.loop);
        clip->SetRootMotion(split.rootMotion);

        clips.PushBack(std::move(clip));
    }

    for (const Handle<Animation>& clip : clips)
    {
        AssetImportInfo clipImportInfo = importInfo;
        clipImportInfo.sourceClip = clip->GetName();

        clip->SetImportInfo(clipImportInfo);
    }

    switch (settings.rigMode)
    {
    case ImportRigMode::UseExisting:
        if (!targetSkeleton.IsValid())
        {
            HYP_LOG(Assets, Warning, "Import of '{}': target skeleton '{}' not found, creating a new one", importInfo.sourcePath, settings.targetSkeleton);

            break;
        }

        contents.prefab.Reset();
        contents.skeleton = targetSkeleton;
        contents.animations = std::move(clips);
        contents.numUnmatchedTracks = 0;

        for (const Handle<Animation>& clip : contents.animations)
        {
            contents.numUnmatchedTracks += clip->NumTracks() - targetSkeleton->CountBoundTracks(clip);
        }

        return;

    case ImportRigMode::None:
        if (contents.prefab.IsValid() && contents.prefab->GetRoot().IsValid())
        {
            ForEachSkinnedEntity(*contents.prefab->GetRoot(), [](Entity& entity, MeshComponent& meshComponent)
                {
                    meshComponent.skeleton = Handle<Skeleton>::empty;

                    if (entity.HasComponent<AnimationComponent>())
                    {
                        entity.RemoveComponent<AnimationComponent>();
                    }
                });
        }

        contents.skeleton.Reset();
        contents.animations.Clear();

        break;

    default:
        break;
    }

    if (contents.skeleton.IsValid())
    {
        contents.skeleton->SetAnimations(clips);
        contents.skeleton->SetImportInfo(importInfo);
        contents.animations = std::move(clips);
    }

    if (!contents.prefab.IsValid() || !contents.prefab->GetRoot().IsValid())
    {
        return;
    }

    contents.prefab->SetImportInfo(importInfo);

    if (settings.scale != 1.0f)
    {
        const Handle<Entity>& root = contents.prefab->GetRoot();
        root->SetLocalScale(root->GetLocalScale() * settings.scale);
    }

    const Handle<Animation> firstClip = contents.animations.Any() ? contents.animations[0] : Handle<Animation>::empty;

    ForEachSkinnedEntity(*contents.prefab->GetRoot(), [&firstClip](Entity& entity, MeshComponent&)
        {
            AnimationComponent* animationComponent = entity.TryGetComponent<AnimationComponent>();

            if (animationComponent && !animationComponent->animation.IsValid())
            {
                animationComponent->animation = firstClip;
            }
        });
}

TResult<ModelImportContents> LoadModelForImport(const AssetImportInfo& importInfo, const Handle<Skeleton>& targetSkeleton)
{
    auto loadResult = AssetManager::GetInstance()->Load<Prefab>(importInfo.sourcePath, String::empty, AssetLoadHint::Transient);

    if (!loadResult.HasValue())
    {
        return HYP_MAKE_ERROR(Error, "Failed to load '{}' for import", importInfo.sourcePath);
    }

    ModelImportContents contents = GatherModelImportContents(loadResult->Result());

    if (!contents.prefab.IsValid())
    {
        return HYP_MAKE_ERROR(Error, "'{}' did not load as a model", importInfo.sourcePath);
    }

    if (importInfo.settings.generateMeshLods && MeshLodGenerator::IsSupported())
    {
        auto generateMeshLods = [](const Handle<AssetObject>& assetObject)
        {
            if (assetObject.IsValid() && assetObject->IsA(Mesh::StaticClass()))
            {
                MeshLodGenerator::GenerateIfMissing(static_cast<Mesh*>(assetObject.Get()));
            }
        };

        AssetRegistry::WalkAssetDeep(BoxedValue(contents.prefab), generateMeshLods);
    }

    ApplyModelImportSettings(contents, importInfo, targetSkeleton);

    return contents;
}

void RegisterModelImport(AssetRegistry& registry, const ModelImportContents& contents)
{
    if (contents.prefab.IsValid())
    {
        registry.PutAssetUnique(contents.prefab);
        registry.PutAssetsDeep(contents.prefab);

        return;
    }

    if (!contents.skeleton.IsValid())
    {
        return;
    }

    Array<Handle<Animation>> skeletonClips = contents.skeleton->GetAnimations();

    for (const Handle<Animation>& clip : contents.animations)
    {
        registry.PutAssetsDeep(clip);

        if (!skeletonClips.Contains(clip))
        {
            skeletonClips.PushBack(clip);
        }
    }

    contents.skeleton->SetAnimations(skeletonClips);
    contents.skeleton->MarkDirty();
}

void MergeModelReimport(AssetRegistry& registry, ModelImportContents& fresh, const Handle<Prefab>& existingPrefab, const Handle<Skeleton>& existingSkeleton)
{
    Handle<Skeleton> skeleton = existingSkeleton.IsValid() ? existingSkeleton : fresh.skeleton;

    if (existingSkeleton.IsValid() && fresh.skeleton.IsValid() && fresh.skeleton != existingSkeleton)
    {
        // the bone tree changes owner; the fresh skeleton would otherwise detach it when it's destroyed
        Handle<Bone> rootBone = fresh.skeleton->GetRootBone();
        fresh.skeleton->SetRootBone(Handle<Bone>::empty);

        existingSkeleton->SetRootBone(rootBone);
        existingSkeleton->SetImportInfo(fresh.skeleton->GetImportInfo());
    }

    Array<Handle<Animation>> mergedClips;
    Map<const Animation*, Handle<Animation>> mergedClipsByFreshClip;

    if (skeleton.IsValid())
    {
        Array<Handle<Animation>> skeletonClips = skeleton->GetAnimations();

        for (const Handle<Animation>& freshClip : fresh.animations)
        {
            const Handle<Animation>& existingClip = FindPreviousImport(skeletonClips, *freshClip);

            if (!existingClip.IsValid() || existingClip == freshClip)
            {
                registry.PutAssetsDeep(freshClip);

                if (!skeletonClips.Contains(freshClip))
                {
                    skeletonClips.PushBack(freshClip);
                }

                mergedClips.PushBack(freshClip);
                mergedClipsByFreshClip[freshClip.Get()] = freshClip;

                continue;
            }

            for (const Handle<AnimationTrack>& oldTrack : existingClip->GetTracks())
            {
                registry.RemoveAsset(oldTrack);
            }

            existingClip->SetTracks(freshClip->GetTracks());
            existingClip->SetLooping(freshClip->IsLooping());
            existingClip->SetRootMotion(freshClip->HasRootMotion());
            existingClip->SetImportInfo(freshClip->GetImportInfo());

            for (const Handle<AnimationTrack>& track : existingClip->GetTracks())
            {
                registry.PutAssetUnique(track);
            }

            existingClip->MarkDirty();

            mergedClips.PushBack(existingClip);
            mergedClipsByFreshClip[freshClip.Get()] = existingClip;
        }

        skeleton->SetAnimations(skeletonClips);
        skeleton->MarkDirty();
    }

    fresh.animations = mergedClips;

    if (!existingPrefab.IsValid() || !fresh.prefab.IsValid() || !fresh.prefab->GetRoot().IsValid())
    {
        return;
    }

    Array<Handle<Mesh>> existingMeshes;
    Array<Handle<Material>> existingMaterials;

    if (existingPrefab->GetRoot().IsValid())
    {
        ForEachMeshEntity(*existingPrefab->GetRoot(), [&](Entity&, MeshComponent& meshComponent)
            {
                if (meshComponent.mesh.IsValid())
                {
                    existingMeshes.PushBack(meshComponent.mesh);
                }

                if (meshComponent.material.IsValid())
                {
                    existingMaterials.PushBack(meshComponent.material);
                }
            });
    }

    ForEachMeshEntity(*fresh.prefab->GetRoot(), [&](Entity& entity, MeshComponent& meshComponent)
        {
            if (meshComponent.skeleton.IsValid() && meshComponent.skeleton == fresh.skeleton)
            {
                meshComponent.skeleton = skeleton;
            }

            for (const Handle<Material>& existingMaterial : existingMaterials)
            {
                if (meshComponent.material.IsValid() && existingMaterial->GetName() == meshComponent.material->GetName())
                {
                    meshComponent.material = existingMaterial;

                    break;
                }
            }

            for (const Handle<Mesh>& existingMesh : existingMeshes)
            {
                if (meshComponent.mesh.IsValid() && existingMesh->GetName() == meshComponent.mesh->GetName() && existingMesh != meshComponent.mesh)
                {
                    // same path and UUID, so references saved against the old mesh resolve to this one
                    meshComponent.mesh->SetUUID(existingMesh->GetUUID());
                    registry.PutAsset(meshComponent.mesh);

                    break;
                }
            }

            if (AnimationComponent* animationComponent = entity.TryGetComponent<AnimationComponent>(); animationComponent && animationComponent->animation.IsValid())
            {
                if (auto mergedIt = mergedClipsByFreshClip.Find(animationComponent->animation.Get()); mergedIt != mergedClipsByFreshClip.End())
                {
                    animationComponent->animation = mergedIt->second;
                }
            }
        });

    existingPrefab->SetRoot(fresh.prefab->GetRoot());
    existingPrefab->SetImportInfo(fresh.prefab->GetImportInfo());

    registry.PutAssetsDeep(existingPrefab);

    existingPrefab->MarkDirty();

    fresh.prefab = existingPrefab;
    fresh.skeleton = skeleton;

    Prefab::OnPrefabChanged(existingPrefab.Get());
}

} // namespace Hyperion
