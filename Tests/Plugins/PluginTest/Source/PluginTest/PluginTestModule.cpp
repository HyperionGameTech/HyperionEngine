/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#include <PluginTest/PluginTestSystem.hpp>
#include <PluginTest/PluginTestImportProcessor.hpp>

#include <Plugins/PluginModule.hpp>
#include <Plugins/PluginContext.hpp>
#include <Plugins/PluginManager.hpp>

#include <Asset/AssetRegistry.hpp>
#include <Asset/Assets.hpp>

#include <Rendering/Material.hpp>

namespace Hyperion {

class PluginTestModule final : public IPluginModule
{
public:
    void OnLoad(PluginContext& context) override
    {
        context.RegisterWorldSystem<PluginTestSystem>();
        context.RegisterModelImportProcessor<PluginTestImportProcessor>();

        // the plugin's Content joins the engine registry, or the cook put it with the engine's content
        const Handle<AssetRegistry> engineRegistry = GetEngineAssetRegistry();
        const Handle<Material> material = engineRegistry.IsValid() ? engineRegistry->GetAsset<Material>(AssetBuckets::Materials, NAME("PluginTestMaterial")) : Handle<Material>::empty;

        if (material.IsValid())
        {
            const Color albedo = material->GetParameters().albedo;

            HYP_LOG(Plugins, Info, "PluginTest content: PluginTestMaterial albedo ({}, {}, {})", albedo.GetRed(), albedo.GetGreen(), albedo.GetBlue());
        }
        else
        {
            HYP_LOG(Plugins, Warning, "PluginTest content: PluginTestMaterial not found");
        }
    }
};

} // namespace Hyperion

HYP_DEFINE_PLUGIN_MODULE(PluginTest, PluginTest, ::Hyperion::PluginTestModule)
