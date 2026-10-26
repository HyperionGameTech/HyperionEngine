/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#include <Editor/EditorImport.hpp>
#include <Editor/EditorSubsystem.hpp>
#include <Editor/EditorTask.hpp>
#include <Editor/EditorProject.hpp>
#include <Editor/EditorAssetFolders.hpp>
#include <Editor/EditorModelImportReview.hpp>
#include <Editor/Tasks/EditorTasks.hpp>

#include <Asset/Assets.hpp>
#include <Asset/AssetBatch.hpp>
#include <Asset/AssetRegistry.hpp>
#include <Asset/AssetObject.hpp>

#include <Scene/Prefab.hpp>
#include <Scene/Entity.hpp>
#include <Scene/EntityManager.hpp>

#include <Scene/Components/MeshComponent.hpp>

#include <Scene/Animation/Skeleton.hpp>
#include <Scene/Animation/Animation.hpp>

#include <Core/Math/MathUtil.hpp>
#include <Core/Logging/Logger.hpp>

namespace Hyperion {

HYP_DECLARE_LOG_CHANNEL(Editor);

static void FileImportedAssets(
    EditorAssetFolders& assetFolders,
    const Handle<AssetObject>& rootAsset,
    const String& modelFolder,
    Set<uint32, EditorAllocator>& changedBuckets)
{
    auto onSubAssetFound = [&](const Handle<AssetObject>& subAsset)
    {
        if (!subAsset.IsValid() || subAsset == rootAsset || !subAsset->IsRegistered())
        {
            return;
        }

        const AssetPath& subAssetPath = subAsset->GetPath();

        if (subAssetPath.registryId != AssetRegistryId::Game || assetFolders.HasAssetFolder(subAssetPath))
        {
            return;
        }

        assetFolders.SetAssetFolder(subAssetPath, EditorAssetFolders::JoinFolderPath(modelFolder, subAssetPath.GetBucket().GetName()));

        changedBuckets.Insert(subAssetPath.GetBucket().GetIndex());
    };

    AssetRegistry::WalkAssetDeep(BoxedValue(rootAsset), onSubAssetFound);
}

Array<Handle<AssetObject>> CommitEditorModelImport(
    EditorSubsystem& subsystem,
    const ModelImportContents& contents,
    const AssetImportInfo& importInfo,
    const String& targetFolder)
{
    AssetRegistry& registry = *GetCurrentAssetRegistry();

    const bool isNewSkeleton = contents.skeleton.IsValid() && !contents.skeleton->IsRegistered();

    RegisterModelImport(registry, contents);

    Array<Handle<AssetObject>> importedAssets;

    if (contents.prefab.IsValid())
    {
        importedAssets.PushBack(contents.prefab);
    }

    // a rig no skinned mesh uses isn't reached through the prefab
    if (!contents.prefab.IsValid() || FindModelSkeleton(*contents.prefab) != contents.skeleton)
    {
        if (isNewSkeleton)
        {
            importedAssets.PushBack(contents.skeleton);
        }

        for (const Handle<Animation>& clip : contents.animations)
        {
            importedAssets.PushBack(clip);
        }
    }

    Set<uint32, EditorAllocator> changedBuckets;

    for (const Handle<AssetObject>& asset : importedAssets)
    {
        if (asset->GetPath().IsValid())
        {
            changedBuckets.Insert(asset->GetPath().GetBucket().GetIndex());
        }
    }

    if (contents.skeleton.IsValid() && contents.skeleton->GetPath().IsValid())
    {
        changedBuckets.Insert(contents.skeleton->GetPath().GetBucket().GetIndex());
    }

    Handle<EditorAssetFolders> assetFolders = subsystem.GetCurrentProject().IsValid()
        ? subsystem.GetCurrentProject()->GetAssetFolders()
        : Handle<EditorAssetFolders>::empty;

    if (assetFolders.IsValid() && importedAssets.Any())
    {
        // an animation-only import has no prefab to name its folder after, so it takes the file's name
        const String folderName = contents.prefab.IsValid()
            ? String(*contents.prefab->GetName())
            : String(StringUtil::StripExtension(FilePath(importInfo.sourcePath).Basename()));

        const String modelFolder = EditorAssetFolders::JoinFolderPath(targetFolder, assetFolders->MakeUniqueFolderName(targetFolder, folderName));

        for (const Handle<AssetObject>& asset : importedAssets)
        {
            const String assetFolder = contents.prefab.IsValid()
                ? modelFolder
                : EditorAssetFolders::JoinFolderPath(modelFolder, asset->GetPath().GetBucket().GetName());

            assetFolders->SetAssetFolder(asset->GetPath(), assetFolder);

            FileImportedAssets(*assetFolders, asset, modelFolder, changedBuckets);
        }
    }

    for (uint32 bucketIndex : changedBuckets)
    {
        subsystem.OnAssetsChanged(bucketIndex);
    }

    return importedAssets;
}

void ImportEditorContent(
    EditorSubsystem* subsystem,
    const Array<FilePath>& files,
    const String& targetFolder,
    EditorImportCallback&& onImported,
    bool reviewModels)
{
    if (files.Empty())
    {
        return;
    }

    struct ImportProgressContext
    {
        AtomicVar<uint32> numProcessed { 0 };
        AtomicVar<uint32> numFailed { 0 };
        uint32 numTotal = 0;
        EditorTaskBase* task = nullptr;
        EditorImportCallback onImported;
    };

    auto context = MakeShared<ImportProgressContext>();
    context->numTotal = uint32(files.Size());
    context->onImported = std::move(onImported);

    EditorTaskScope* editorTaskScope = new EditorTaskScope(
        TickableEditorTask::StaticClass(),
        [context]()
        {
            const uint32 numProcessed = context->numProcessed.Get(MemoryOrder::RELAXED);
            const uint32 numFailed = context->numFailed.Get(MemoryOrder::RELAXED);

            if (EditorTaskBase* task = context->task)
            {
                task->SetProgress(context->numTotal > 0
                        ? MathUtil::Min(float(numProcessed) / float(context->numTotal), 1.0f)
                        : 1.0f);

                task->SetDescription(numFailed > 0
                        ? HYP_FORMAT("Importing {}/{} assets ({} failed)", numProcessed, context->numTotal, numFailed)
                        : HYP_FORMAT("Importing {}/{} assets", numProcessed, context->numTotal));
            }
        },
        "Importing content...",
        HYP_FORMAT("Importing 0/{} assets", context->numTotal),
        /* isForegroundTask */ true);

    context->task = editorTaskScope->GetEditorTask();

    // Create identifier based on the common folder of the assets
    const String identifier = files[0].BasePath().Basename();

    AssetBatch* batch = AssetManager::GetInstance()->CreateBatch(identifier);

    Map<String, FilePath> sourcePathsByKey;

    for (const FilePath& file : files)
    {
        // models stay unregistered until their import settings, mesh LODs included, are applied
        const EnumFlags<AssetLoadHint> hint = IsEditorImportModelFile(file)
            ? EnumFlags<AssetLoadHint>(AssetLoadHint::Transient)
            : GetEditorImportLoadHint(file);

        batch->Add(file.Basename(), file, hint);

        sourcePathsByKey[file.Basename()] = file;
    }

    batch->GetCallbacks().OnItemComplete
        .Bind([context](const AssetBatchCallbackData&)
              {
                  context->numProcessed.Increment(1, MemoryOrder::RELEASE);
              })
        .Detach();

    batch->GetCallbacks().OnItemFailed
        .Bind([context](const AssetBatchCallbackData&)
              {
                  context->numFailed.Increment(1, MemoryOrder::RELEASE);
                  context->numProcessed.Increment(1, MemoryOrder::RELEASE);
              })
        .Detach();

    const String normalizedTargetFolder = EditorAssetFolders::NormalizeFolderPath(targetFolder);

    batch->OnComplete
        .Bind([context, editorTaskScope, weakSubsystem = MakeWeakRef(subsystem), normalizedTargetFolder, sourcePathsByKey = std::move(sourcePathsByKey), reviewModels](AssetMap& results) mutable
              {
                  HYP_LOG(Editor, Verbose, "{} assets loaded.", results.Size());

                  String postText;
                  int numFailed = 0;
                  uint32 numProcessed = 0;

                  Handle<EditorSubsystem> subsystem = weakSubsystem.Lock();

                  Handle<EditorAssetFolders> assetFolders;

                  if (subsystem.IsValid() && subsystem->GetCurrentProject().IsValid())
                  {
                      assetFolders = subsystem->GetCurrentProject()->GetAssetFolders();
                  }

                  Array<Handle<AssetObject>> importedAssets;
                  Set<uint32, EditorAllocator> changedBuckets;

                  for (auto& it : results)
                  {
                      String& key = it.first;
                      LoadedAsset& loadedAsset = it.second;

                      editorTaskScope->GetEditorTask()->SetDescription(HYP_FORMAT("Processing {}/{}: {}", ++numProcessed, results.Size(), key) + postText);

                      if (!loadedAsset.IsValid())
                      {
                          HYP_LOG(Editor, Error, "Failed to import asset '{}': {}", key, loadedAsset.GetError().GetMessage());

                          postText = HYP_FORMAT(" ({} failed)", ++numFailed);

                          editorTaskScope->GetEditorTask()->SetDescription(HYP_FORMAT("Processing {}/{}: {}", numProcessed, results.Size(), key) + postText);

                          continue;
                      }

                      Handle<AssetObject> assetObject = loadedAsset.ExtractAs<AssetObject>();
                      if (!assetObject.IsValid())
                      {
                          continue;
                      }

                      if (Handle<Prefab> prefab = TryCast<Prefab>(assetObject); prefab.IsValid() && subsystem.IsValid() && IsEditorImportModelFile(FilePath(key)))
                      {
                          ModelImportContents contents = GatherModelImportContents(prefab);

                          AssetImportInfo importInfo;

                          if (auto sourcePathIt = sourcePathsByKey.Find(key); sourcePathIt != sourcePathsByKey.End())
                          {
                              importInfo.sourcePath = sourcePathIt->second;
                          }

                          if (reviewModels && contents.skeleton.IsValid())
                          {
                              Handle<EditorModelImportReview> review = MakeHandle<EditorModelImportReview>(std::move(contents), importInfo, normalizedTargetFolder);
                              InitObject(review);

                              subsystem->QueueModelImportReview(review);

                              continue;
                          }

                          if (Result applyResult = ApplyModelImportSettings(contents, importInfo, Handle<Skeleton>::empty); applyResult.HasError())
                          {
                              HYP_LOG(Editor, Error, "Failed to import asset '{}': {}", key, applyResult.GetError().GetMessage());

                              postText = HYP_FORMAT(" ({} failed)", ++numFailed);

                              continue;
                          }

                          importedAssets.Concat(CommitEditorModelImport(*subsystem, contents, importInfo, normalizedTargetFolder));

                          continue;
                      }

                      GetCurrentAssetRegistry()->PutAssetUnique(assetObject);

                      importedAssets.PushBack(assetObject);

                      const AssetPath& assetPath = assetObject->GetPath();

                      if (!assetPath.IsValid())
                      {
                          continue;
                      }

                      changedBuckets.Insert(assetPath.GetBucket().GetIndex());

                      if (assetFolders.IsValid())
                      {
                          assetFolders->SetAssetFolder(assetPath, normalizedTargetFolder);
                      }
                  }

                  if (subsystem.IsValid())
                  {
                      for (uint32 bucketIndex : changedBuckets)
                      {
                          subsystem->OnAssetsChanged(bucketIndex);
                      }
                  }

                  delete editorTaskScope;

                  if (subsystem.IsValid() && context->onImported.IsValid())
                  {
                      context->onImported(*subsystem, importedAssets);
                  }
              })
        .Detach();

    batch->LoadAsync();

    // Note: The batch will be destroyed automatically by AssetManager when complete
}

const AssetImportInfo* GetEditorAssetImportInfo(const AssetObject* asset)
{
    const AssetImportInfo* importInfo = nullptr;

    if (const Prefab* prefab = TryCast<Prefab>(asset))
    {
        importInfo = &prefab->GetImportInfo();
    }
    else if (const Skeleton* skeleton = TryCast<Skeleton>(asset))
    {
        importInfo = &skeleton->GetImportInfo();
    }
    else if (const Animation* animation = TryCast<Animation>(asset))
    {
        importInfo = &animation->GetImportInfo();
    }

    return importInfo && importInfo->IsValid() ? importInfo : nullptr;
}

template <class T, class Predicate>
static Handle<T> FindRegisteredAsset(const AssetBucket& bucket, Predicate&& predicate)
{
    Array<AssetDesc> assetDescs;
    GetCurrentAssetRegistry()->GetBucketAssetDescs(bucket.GetIndex(), assetDescs);

    for (const AssetDesc& assetDesc : assetDescs)
    {
        Handle<T> asset = GetCurrentAssetRegistry()->GetAsset<T>(bucket, assetDesc.name);

        if (asset.IsValid() && predicate(*asset))
        {
            return asset;
        }
    }

    return Handle<T>::empty;
}

bool ReimportEditorAsset(EditorSubsystem& subsystem, const Handle<AssetObject>& asset)
{
    AssertOnThread(g_simThread);

    const AssetImportInfo* storedImportInfo = GetEditorAssetImportInfo(asset.Get());

    if (!storedImportInfo)
    {
        HYP_LOG(Editor, Warning, "Cannot reimport '{}': it wasn't imported from a file", asset.IsValid() ? asset->GetName() : Name::Invalid());

        return false;
    }

    const AssetImportInfo importInfo = *storedImportInfo;
    const bool usesExistingSkeleton = importInfo.settings.rigMode == ImportRigMode::UseExisting;

    const auto isFromSameSource = [&importInfo](const auto& candidate)
    {
        return candidate.GetImportInfo().sourcePath == importInfo.sourcePath;
    };

    Handle<Prefab> prefab = TryCast<Prefab>(asset);
    Handle<Skeleton> skeleton = TryCast<Skeleton>(asset);

    if (!prefab.IsValid() && !usesExistingSkeleton)
    {
        prefab = FindRegisteredAsset<Prefab>(AssetBuckets::Prefabs, isFromSameSource);
    }

    if (usesExistingSkeleton)
    {
        skeleton = GetCurrentAssetRegistry()->GetAsset<Skeleton>(AssetBuckets::Skeletons, importInfo.settings.targetSkeleton);
    }
    else
    {
        if (!skeleton.IsValid() && prefab.IsValid())
        {
            skeleton = FindModelSkeleton(*prefab);
        }

        // a rig with no skinned mesh is registered beside the prefab rather than inside it
        if (!skeleton.IsValid())
        {
            skeleton = FindRegisteredAsset<Skeleton>(AssetBuckets::Skeletons, isFromSameSource);
        }
    }

    auto loadResult = LoadModelForImport(importInfo, usesExistingSkeleton ? skeleton : Handle<Skeleton>::empty);

    if (loadResult.HasError())
    {
        HYP_LOG(Editor, Error, "Reimport failed: {}", loadResult.GetError().GetMessage());

        return false;
    }

    ModelImportContents contents = loadResult.GetValue();

    MergeModelReimport(*GetCurrentAssetRegistry(), contents, prefab, skeleton);

    subsystem.SyncStalePrefabInstances();

    for (const AssetBucket& bucket : { AssetBuckets::Prefabs, AssetBuckets::Skeletons, AssetBuckets::Animations, AssetBuckets::AnimationTracks, AssetBuckets::Meshes, AssetBuckets::Materials })
    {
        subsystem.OnAssetsChanged(bucket.GetIndex());
    }

    HYP_LOG(Editor, Info, "Reimported '{}' ({} clips)", importInfo.sourcePath, contents.animations.Size());

    return true;
}

} // namespace Hyperion
