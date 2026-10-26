/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#pragma once

#include <Core/Containers/Array.hpp>
#include <Core/Containers/String.hpp>

#include <Core/Name/Name.hpp>

#include <Core/Reflection/ObjectMacros.hpp>

#include <Core/Types.hpp>

namespace Hyperion {

HYP_ENUM()
enum class ImportRigMode : uint8
{
    Create = 0,
    UseExisting,
    None
};

HYP_STRUCT()
struct AnimationClipSplit
{
    HYP_STRUCT_BODY(AnimationClipSplit);

    HYP_FIELD(Property = "Name", Serialize = true)
    Name name;

    HYP_FIELD(Property = "Take", Serialize = true)
    Name take;

    HYP_FIELD(Property = "StartFrame", Serialize = true)
    uint32 startFrame = 0;

    HYP_FIELD(Property = "EndFrame", Serialize = true)
    uint32 endFrame = 0;

    HYP_FIELD(Property = "Loop", Serialize = true)
    bool loop = true;

    HYP_FIELD(Property = "RootMotion", Serialize = true)
    bool rootMotion = false;
};

HYP_STRUCT()
struct ModelImportSettings
{
    HYP_STRUCT_BODY(ModelImportSettings);

    HYP_FIELD(Property = "RigMode", Serialize = true)
    ImportRigMode rigMode = ImportRigMode::Create;

    HYP_FIELD(Property = "TargetSkeleton", Serialize = true)
    Name targetSkeleton;

    HYP_FIELD(Property = "Scale", Serialize = true)
    float scale = 1.0f;

    HYP_FIELD(Property = "FrameRate", Serialize = true)
    float frameRate = 30.0f;

    HYP_FIELD(Property = "GenerateMeshLods", Serialize = true)
    bool generateMeshLods = true;

    HYP_FIELD(Property = "AutoMapHumanoid", Serialize = true)
    bool autoMapHumanoid = true;

    HYP_FIELD(Property = "ExcludedTakes", Serialize = true)
    Array<Name> excludedTakes;

    HYP_FIELD(Property = "Splits", Serialize = true)
    Array<AnimationClipSplit> splits;
};

// where an imported asset came from, so Reimport can rerun the import with the same settings
HYP_STRUCT()
struct AssetImportInfo
{
    HYP_STRUCT_BODY(AssetImportInfo);

    HYP_FIELD(Property = "SourcePath", Serialize = true)
    String sourcePath;

    HYP_FIELD(Property = "Settings", Serialize = true)
    ModelImportSettings settings;

    // for a clip, the take or split it was made from; its asset name can gain a suffix when registered
    HYP_FIELD(Property = "SourceClip", Serialize = true)
    Name sourceClip;

    HYP_FORCE_INLINE bool IsValid() const
    {
        return sourcePath.Any();
    }
};

} // namespace Hyperion
