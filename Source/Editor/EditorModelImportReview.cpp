/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#include <EditorPch.hpp>

#include <Editor/EditorModelImportReview.hpp>
#include <Editor/EditorImport.hpp>
#include <Editor/EditorSubsystem.hpp>

#include <Asset/AssetRegistry.hpp>

#include <Scene/Prefab.hpp>

#include <Scene/Animation/Skeleton.hpp>
#include <Scene/Animation/Animation.hpp>

#include <Core/Math/MathUtil.hpp>
#include <Core/Logging/Logger.hpp>

#include <cmath>

#include <EditorModelImportReview.generated.inl>

namespace Hyperion {

HYP_DECLARE_LOG_CHANNEL(Editor);

EditorModelImportReview::EditorModelImportReview() = default;

EditorModelImportReview::EditorModelImportReview(ModelImportContents&& contents, const AssetImportInfo& importInfo, const String& targetFolder)
    : m_contents(std::move(contents)),
      m_importInfo(importInfo),
      m_targetFolder(targetFolder)
{
    m_takes = m_contents.animations;
}

EditorModelImportReview::~EditorModelImportReview() = default;

String EditorModelImportReview::GetFileName() const
{
    return FilePath(m_importInfo.sourcePath).Basename();
}

bool EditorModelImportReview::HasMesh() const
{
    return m_contents.prefab.IsValid() && m_contents.prefab->CountMeshes() != 0;
}

uint32 EditorModelImportReview::GetBoneCount() const
{
    return m_contents.skeleton.IsValid() ? uint32(m_contents.skeleton->GetBoneNames().Size()) : 0;
}

Array<Name> EditorModelImportReview::GetTakeIds() const
{
    Array<Name> takeIds;

    for (const Handle<Animation>& take : m_takes)
    {
        takeIds.PushBack(take->GetImportInfo().sourceClip);
    }

    return takeIds;
}

const Handle<Animation>& EditorModelImportReview::GetTake(Name takeName) const
{
    for (const Handle<Animation>& take : m_takes)
    {
        if (take->GetImportInfo().sourceClip == takeName)
        {
            return take;
        }
    }

    return Handle<Animation>::Null();
}

Name EditorModelImportReview::GetTakeDisplayName(Name takeName) const
{
    return GetTake(takeName)->GetName();
}

uint32 EditorModelImportReview::GetTakeFrameCount(Name takeName) const
{
    return uint32(MathUtil::Round(GetTake(takeName)->GetLength() * m_contents.frameRate));
}

Array<Name> EditorModelImportReview::GetSkeletonNames() const
{
    Array<AssetDesc> skeletonDescs;
    GetCurrentAssetRegistry()->GetBucketAssetDescs(AssetBuckets::Skeletons.GetIndex(), skeletonDescs);

    Array<Name> skeletonNames;

    for (const AssetDesc& skeletonDesc : skeletonDescs)
    {
        skeletonNames.PushBack(skeletonDesc.name);
    }

    return skeletonNames;
}

uint32 EditorModelImportReview::CountUnmatchedTracks(Name skeletonName) const
{
    Handle<Skeleton> skeleton = GetCurrentAssetRegistry()->GetAsset<Skeleton>(AssetBuckets::Skeletons, skeletonName);

    if (!skeleton.IsValid())
    {
        return 0;
    }

    uint32 numUnmatched = 0;

    for (const Handle<Animation>& take : m_takes)
    {
        numUnmatched += take->NumTracks() - skeleton->CountBoundTracks(take);
    }

    return numUnmatched;
}

void EditorModelImportReview::SetRigMode(uint32 rigMode)
{
    m_importInfo.settings.rigMode = ImportRigMode(MathUtil::Min(rigMode, uint32(ImportRigMode::None)));
}

void EditorModelImportReview::SetTargetSkeleton(Name skeletonName)
{
    m_importInfo.settings.targetSkeleton = skeletonName;
}

void EditorModelImportReview::SetScale(float scale)
{
    m_importInfo.settings.scale = MathUtil::Max(scale, 0.0001f);
}

void EditorModelImportReview::SetGenerateMeshLods(bool generateMeshLods)
{
    m_importInfo.settings.generateMeshLods = generateMeshLods;
}

void EditorModelImportReview::SetAutoMapHumanoid(bool autoMapHumanoid)
{
    m_importInfo.settings.autoMapHumanoid = autoMapHumanoid;
}

void EditorModelImportReview::SetTakeIncluded(Name takeName, bool included)
{
    Array<Name>& excludedTakes = m_importInfo.settings.excludedTakes;

    if (included)
    {
        excludedTakes.Erase(takeName);
    }
    else if (!excludedTakes.Contains(takeName))
    {
        excludedTakes.PushBack(takeName);
    }
}

void EditorModelImportReview::ClearSplits()
{
    m_importInfo.settings.splits.Clear();
}

void EditorModelImportReview::AddSplit(Name name, Name takeName, uint32 startFrame, uint32 endFrame, bool loop, bool rootMotion)
{
    m_importInfo.settings.splits.PushBack(AnimationClipSplit { name, takeName, startFrame, MathUtil::Max(endFrame, startFrame), loop, rootMotion });
}

bool EditorModelImportReview::Commit(EditorSubsystem* subsystem)
{
    AssertOnThread(g_simThread);

    if (m_isFinished || !subsystem)
    {
        return false;
    }

    m_isFinished = true;

    const Handle<Skeleton> targetSkeleton = m_importInfo.settings.rigMode == ImportRigMode::UseExisting
        ? GetCurrentAssetRegistry()->GetAsset<Skeleton>(AssetBuckets::Skeletons, m_importInfo.settings.targetSkeleton)
        : Handle<Skeleton>::empty;

    if (Result applyResult = ApplyModelImportSettings(m_contents, m_importInfo, targetSkeleton); applyResult.HasError())
    {
        HYP_LOG(Editor, Error, "Import failed: {}", applyResult.GetError().GetMessage());

        m_contents = {};
        m_takes.Clear();

        return false;
    }

    const bool importedAnything = CommitEditorModelImport(*subsystem, m_contents, m_importInfo, m_targetFolder).Any();

    m_contents = {};
    m_takes.Clear();

    return importedAnything;
}

void EditorModelImportReview::Cancel()
{
    m_isFinished = true;

    m_contents = {};
    m_takes.Clear();
}

bool EditorModelImportReview::IsFinished() const
{
    return m_isFinished;
}

} // namespace Hyperion
