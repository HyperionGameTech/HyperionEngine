/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#include <PluginTest/PluginTestImportProcessor.hpp>

#include <Asset/ModelImport.hpp>

#include <Scene/Prefab.hpp>

#include <Plugins/PluginManager.hpp>

#include <PluginTestImportProcessor.generated.inl>

namespace Hyperion {

const Class* PluginTestImportProcessor::GetSettingsClass() const
{
    return PluginTestImportSettings::StaticClass();
}

void PluginTestImportProcessor::Process(ModelImportContents& contents, const AssetImportInfo& importInfo, const ModelImportProcessorSettings* settings)
{
    if (!static_cast<const PluginTestImportSettings*>(settings)->logImports)
    {
        return;
    }

    HYP_LOG(Plugins, Info, "PluginTest import processor: '{}' ({} mesh(es), {} clip(s))",
        importInfo.sourcePath,
        contents.prefab.IsValid() ? contents.prefab->CountMeshes() : 0,
        contents.animations.Size());
}

} // namespace Hyperion
