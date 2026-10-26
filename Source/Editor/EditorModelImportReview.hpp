/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#pragma once

#include <Asset/ModelImport.hpp>

#include <Editor/EditorImportReview.hpp>

#include <Core/Containers/String.hpp>

#include <Editor/EditorMemory.hpp>

namespace Hyperion {

class EditorSubsystem;

// a loaded model held back from the registry while the import panel picks its settings
HYP_CLASS(Serialize = false)
class EDITOR_API EditorModelImportReview final : public EditorImportReview
{
    HYP_OBJECT_BODY(EditorModelImportReview);

public:
    EditorModelImportReview();
    EditorModelImportReview(ModelImportContents&& contents, const AssetImportInfo& importInfo, const String& targetFolder);
    ~EditorModelImportReview() override;

    String GetFileName() const override;
    bool Commit(EditorSubsystem* subsystem) override;
    void Cancel() override;

    HYP_METHOD()
    bool HasMesh() const;

    HYP_METHOD()
    uint32 GetBoneCount() const;

    HYP_METHOD()
    Array<Name> GetTakeIds() const;

    HYP_METHOD()
    Name GetTakeDisplayName(Name takeName) const;

    HYP_METHOD()
    uint32 GetTakeFrameCount(Name takeName) const;

    HYP_METHOD()
    Array<Name> GetSkeletonNames() const;

    HYP_METHOD()
    uint32 CountUnmatchedTracks(Name skeletonName) const;

    HYP_METHOD()
    void SetRigMode(uint32 rigMode);

    HYP_METHOD()
    void SetTargetSkeleton(Name skeletonName);

    HYP_METHOD()
    void SetScale(float scale);

    HYP_METHOD()
    void SetGenerateMeshLods(bool generateMeshLods);

    HYP_METHOD()
    void SetAutoMapHumanoid(bool autoMapHumanoid);

    HYP_METHOD()
    void SetTakeIncluded(Name takeName, bool included);

    HYP_METHOD()
    void ClearSplits();

    HYP_METHOD()
    void AddSplit(Name name, Name takeName, uint32 startFrame, uint32 endFrame, bool loop, bool rootMotion);

private:
    const Handle<Animation>& GetTake(Name takeName) const;

    ModelImportContents m_contents;
    Array<Handle<Animation>> m_takes;
    AssetImportInfo m_importInfo;
    String m_targetFolder;
};

} // namespace Hyperion
