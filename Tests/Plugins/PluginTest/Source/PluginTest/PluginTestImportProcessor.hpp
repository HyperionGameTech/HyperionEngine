/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#pragma once

#include <Asset/ModelImportProcessor.hpp>

namespace Hyperion {

HYP_CLASS(Label = "Plugin Test")
class ENGINE_API PluginTestImportSettings final : public ModelImportProcessorSettings
{
    HYP_OBJECT_BODY(PluginTestImportSettings);

public:
    HYP_FIELD(Property = "LogImports", Serialize = true, Editor = true)
    bool logImports = true;
};

class PluginTestImportProcessor final : public IModelImportProcessor
{
public:
    const Class* GetSettingsClass() const override;

    void Process(ModelImportContents& contents, const AssetImportInfo& importInfo, const ModelImportProcessorSettings* settings) override;
};

} // namespace Hyperion
