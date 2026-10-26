/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#ifdef HYP_TESTS

#include <Framework/Testing/EngineTest.hpp>
#include <Framework/EngineGlobals.hpp>

#include <Asset/AssetRegistry.hpp>
#include <Asset/Assets.hpp>

#include <Rendering/Material.hpp>

#include <Core/FileSystem/FilePath.hpp>

#include <Core/IO/ByteReader.hpp>
#include <Core/IO/ByteWriter.hpp>

#include <Core/Utilities/GlobalContext.hpp>

#include <Core/Math/MathUtil.hpp>

namespace Hyperion {
namespace tests {
namespace asset {

namespace {

const FilePath FixtureMaterial = FilePath(__FILE__).BasePath() / ".." / ".." / "Plugins" / "PluginTest" / "Content" / "Materials" / "PluginTestMaterial.hmf";

// a fresh registry over an empty root, beside a copy of the PluginTest fixture's Content
struct ContentRootScene
{
    FilePath rootDirectory;
    FilePath contentDirectory;
    Handle<AssetRegistry> registry;
};

bool MakeContentRootScene(ContentRootScene& scene)
{
    const FilePath testDirectory = EngineGlobals::GetTempDirectory() / "AssetContentRootTests";

    if (testDirectory.Exists())
    {
        testDirectory.RemoveRecursively();
    }

    scene.rootDirectory = testDirectory / "Root";
    scene.contentDirectory = testDirectory / "PluginContent";

    const FilePath materialsDirectory = scene.contentDirectory / "Materials";

    if (!scene.rootDirectory.MkDir() || !materialsDirectory.MkDir())
    {
        return false;
    }

    FileByteReader reader { FixtureMaterial };
    const ByteBuffer manifest = reader.Read();

    FileByteWriter writer { materialsDirectory / "PluginTestMaterial.hmf" };
    writer.Write(manifest);
    writer.Close();

    // a Game registry, which assets find through the context; an Engine one would be the engine's own
    scene.registry = MakeHandle<AssetRegistry>(AssetRegistryId::Game, scene.rootDirectory);
    scene.registry->Initialize(nullptr);

    return manifest.Size() > 0;
}

} // namespace

HYP_ENGINE_TEST(AssetContentRoot, MountsAndUnmountsAssets)
{
    ContentRootScene scene;
    HYP_TEST_REQUIRE(MakeContentRootScene(scene));

    GlobalContextScope registryScope { AssetRegistryContext { scene.registry } };

    HYP_TEST_CHECK(!scene.registry->GetAsset<Material>(AssetBuckets::Materials, NAME("PluginTestMaterial")).IsValid());

    scene.registry->AddContentRoot(scene.contentDirectory);

    const Handle<Material> material = scene.registry->GetAsset<Material>(AssetBuckets::Materials, NAME("PluginTestMaterial"));
    HYP_TEST_REQUIRE(material.IsValid());
    HYP_TEST_CHECK(MathUtil::Abs(material->GetParameters().albedo.GetGreen() - 0.8f) < 1e-3f);

    HYP_TEST_CHECK(scene.registry->GetAssetDirectory(AssetBuckets::Materials.GetIndex(), NAME("PluginTestMaterial")) == scene.contentDirectory / "Materials");
    HYP_TEST_CHECK(scene.registry->GetAssetDirectory(AssetBuckets::Materials.GetIndex(), NAME("NewMaterial")) == scene.rootDirectory / "Materials");

    scene.registry->RemoveContentRoot(scene.contentDirectory);

    HYP_TEST_CHECK(!scene.registry->GetAsset<Material>(AssetBuckets::Materials, NAME("PluginTestMaterial")).IsValid());
    HYP_TEST_CHECK((scene.contentDirectory / "Materials" / "PluginTestMaterial.hmf").Exists());
}

HYP_ENGINE_TEST(AssetContentRoot, SavesBackToItsRoot)
{
    ContentRootScene scene;
    HYP_TEST_REQUIRE(MakeContentRootScene(scene));

    GlobalContextScope registryScope { AssetRegistryContext { scene.registry } };

    scene.registry->AddContentRoot(scene.contentDirectory);

    const Handle<Material> material = scene.registry->GetAsset<Material>(AssetBuckets::Materials, NAME("PluginTestMaterial"));
    HYP_TEST_REQUIRE(material.IsValid());

    MaterialParameters parameters = material->GetParameters();
    // colours keep 8 bits a channel, so 0.9 saves as 0.898
    parameters.albedo = Color(0.9f, 0.1f, 0.1f, 1.0f);
    material->SetParameters(parameters);
    material->MarkDirty();

    scene.registry->SaveDirtyAssets();

    HYP_TEST_CHECK_MSG(!(scene.rootDirectory / "Materials" / "PluginTestMaterial.hmf").Exists(), "the asset was saved into the registry's root instead of its content root");

    FileByteReader reader { scene.contentDirectory / "Materials" / "PluginTestMaterial.hmf" };
    const ByteBuffer manifest = reader.Read();
    const char* manifestBegin = reinterpret_cast<const char*>(manifest.Data());
    const String manifestText(manifestBegin, manifestBegin + manifest.Size());

    HYP_TEST_CHECK_MSG(manifestText.Contains("Albedo = (0.898"), "the content root's manifest doesn't have the edit:\n{}", manifestText);
}

} // namespace asset
} // namespace tests
} // namespace Hyperion

#endif // HYP_TESTS
