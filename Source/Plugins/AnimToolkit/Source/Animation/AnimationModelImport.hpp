/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <Asset/ModelImportProcessor.hpp>

namespace Hyperion {

HYP_CLASS(Label = "Animation")
class ANIMATION_API AnimationModelImportSettings final : public ModelImportProcessorSettings
{
    HYP_OBJECT_BODY(AnimationModelImportSettings);

public:
    HYP_FIELD(Property = "AddAnimationComponent", Serialize = true, Editor = true)
    bool addAnimationComponent = true;
};

// gives each skinned entity of an imported model an AnimationComponent playing the first clip
class ANIMATION_API AnimationModelImportProcessor final : public IModelImportProcessor
{
public:
    const Class* GetSettingsClass() const override;

    void Process(ModelImportContents& contents, const AssetImportInfo& importInfo, const ModelImportProcessorSettings* settings) override;

    void OnClipsMerged(Node& root, const Map<const Animation*, Handle<Animation>>& mergedClipsByFreshClip) override;
};

} // namespace Hyperion
