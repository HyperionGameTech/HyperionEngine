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

#include <Rendering/Mesh.hpp>
#include <Rendering/Util/MeshBuilder.hpp>
#include <Rendering/Shared.hpp>

#include <Core/FileSystem/FilePath.hpp>

#include <Core/IO/ByteReader.hpp>

#include <Core/Utilities/GlobalContext.hpp>

namespace Hyperion {
namespace tests {
namespace asset {

HYP_ENGINE_TEST(AssetBlobManifest, ReplacedAssetKeepsManifestInStep)
{
    const FilePath rootDirectory = EngineGlobals::GetTempDirectory() / "AssetBlobManifestTests";

    if (rootDirectory.Exists())
    {
        rootDirectory.RemoveRecursively();
    }

    HYP_TEST_REQUIRE(rootDirectory.MkDir());

    Handle<AssetRegistry> registry = MakeHandle<AssetRegistry>(AssetRegistryId::Game, rootDirectory);
    registry->Initialize(nullptr);

    GlobalContextScope registryScope { AssetRegistryContext { registry } };

    const Name meshName = NAME("SwappedMesh");

    Handle<Mesh> original = MeshBuilder::Quad();
    HYP_TEST_REQUIRE(!original->Rename(meshName).HasError());

    registry->PutAsset(original);
    registry->SaveDirtyAssets();

    const FilePath meshDirectory = registry->GetAssetDirectory(AssetBuckets::Meshes.GetIndex(), meshName);
    HYP_TEST_REQUIRE((meshDirectory / "SwappedMesh.hmf").Exists());

    Handle<Mesh> replacement = MeshBuilder::NormalizedCubeSphere(8);
    HYP_TEST_REQUIRE(!replacement->Rename(meshName).HasError());
    HYP_TEST_REQUIRE(replacement->NumIndices(0) != original->NumIndices(0));

    registry->PutAsset(replacement);

    registry->RemoveCached(AssetBuckets::Meshes, meshName);

    const Handle<Mesh> reloaded = registry->GetAsset<Mesh>(AssetBuckets::Meshes, meshName);
    HYP_TEST_REQUIRE(reloaded.IsValid());

    FileByteReader indexBlob { meshDirectory / (String(*meshName) + "." + Mesh::GetIndexBufferBlobMagic(0) + ".raw.blob") };

    HYP_TEST_CHECK_MSG(reloaded->NumIndices(0) == replacement->NumIndices(0),
        "the manifest on disk still describes the replaced mesh ({} indices, expected {})", reloaded->NumIndices(0), replacement->NumIndices(0));
    HYP_TEST_CHECK_MSG(indexBlob.Max() == reloaded->NumIndices(0) * GpuElemTypeSize(reloaded->GetMeshAttributes().indexBufferElemType),
        "the index blob is {} bytes but the manifest describes {} indices", indexBlob.Max(), reloaded->NumIndices(0));
}

} // namespace asset
} // namespace tests
} // namespace Hyperion

#endif // HYP_TESTS
