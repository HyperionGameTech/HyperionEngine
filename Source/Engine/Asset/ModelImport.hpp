/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#pragma once

#include <Asset/AssetImportInfo.hpp>

#include <Core/Containers/Array.hpp>

#include <Core/Reflection/Handle.hpp>

#include <Core/Utilities/Result.hpp>

namespace Hyperion {

class AssetRegistry;
class Prefab;
class Skeleton;
class Animation;

// a loaded model before it's registered: the prefab, its skeleton and clips, all unregistered
struct ModelImportContents
{
    Handle<Prefab> prefab;
    Handle<Skeleton> skeleton;
    Array<Handle<Animation>> animations;
    float frameRate = 30.0f;
    uint32 numUnmatchedTracks = 0;

    HYP_FORCE_INLINE bool IsAnimationOnly() const
    {
        return !prefab.IsValid() && animations.Any();
    }
};

ENGINE_API Handle<Skeleton> FindModelSkeleton(const Prefab& prefab);

ENGINE_API ModelImportContents GatherModelImportContents(const Handle<Prefab>& prefab);

ENGINE_API Handle<Animation> MakeSubClip(const Animation& source, Name name, float startTime, float endTime);

/*! \brief Applies takes, splits, the rig mode and scale to freshly loaded contents and tags everything with importInfo.
 *  With ImportRigMode::UseExisting, targetSkeleton receives the clips and the prefab is dropped. Takes and splits
 *  name takes by their source clip. */
ENGINE_API Result ApplyModelImportSettings(ModelImportContents& contents, const AssetImportInfo& importInfo, const Handle<Skeleton>& targetSkeleton);

/*! \brief Loads importInfo.sourcePath without registering anything, then applies importInfo's settings. */
ENGINE_API TResult<ModelImportContents> LoadModelForImport(const AssetImportInfo& importInfo, const Handle<Skeleton>& targetSkeleton);

/*! \brief Registers imported contents; animation-only contents are added to their target skeleton's clip list. */
ENGINE_API void RegisterModelImport(AssetRegistry& registry, const ModelImportContents& contents);

/*! \brief Moves freshly loaded contents into the assets a previous import made, keeping their identities:
 *  the skeleton takes the new bones (keeping its sockets), clips match by source clip and take the new tracks
 *  (keeping notifies), and the prefab takes the new hierarchy, reusing existing materials and mesh UUIDs by name. */
ENGINE_API void MergeModelReimport(
    AssetRegistry& registry,
    ModelImportContents& fresh,
    const Handle<Prefab>& existingPrefab,
    const Handle<Skeleton>& existingSkeleton);

} // namespace Hyperion
