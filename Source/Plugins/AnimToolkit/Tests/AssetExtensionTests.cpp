/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#ifdef HYP_TESTS

#include <Framework/Testing/EngineTest.hpp>
#include <Framework/EngineGlobals.hpp>

#include <Asset/AssetRegistry.hpp>
#include <Asset/AssetExtension.hpp>
#include <Asset/ModelImport.hpp>
#include <Asset/SerializationUtils.hpp>

#include <Scene/Animation/Skeleton.hpp>
#include <Animation/HumanoidMapping.hpp>

#include <Core/DataProcessing/HMF/HMF.hpp>

#include <Core/FileSystem/FilePath.hpp>

#include <Core/Utilities/GlobalContext.hpp>

namespace Hyperion {
namespace tests {
namespace scene {

namespace {

// a property line added at the top of the written skeleton's block; an empty Extensions list isn't written at all
String InsertProperty(const String& skeletonText, const String& propertyText)
{
    const size_t insertAt = skeletonText.FindFirstIndex(UTF8StringView("{\n")) + 2;

    return String(skeletonText.Substr(0, insertAt)) + propertyText + "\n" + String(skeletonText.Substr(insertAt));
}

Handle<AssetRegistry> MakeScratchRegistry()
{
    Handle<AssetRegistry> scratchRegistry = MakeHandle<AssetRegistry>(AssetRegistryId::Game, EngineGlobals::GetTempDirectory() / "AssetExtensionTests");
    scratchRegistry->Initialize(nullptr);

    return scratchRegistry;
}

// TwoTakeRig's bones are Root, Mid and Tip, none of which auto-map, so a mapped Hips came from the test
Handle<Skeleton> ImportRigSkeleton()
{
    AssetImportInfo importInfo;
    importInfo.sourcePath = (FilePath(__FILE__).BasePath() / ".." / ".." / ".." / ".." / "Tests" / "Engine" / "Data" / "Animation" / "TwoTakeRig.glb");
    importInfo.settings.autoMapHumanoid = false;

    auto importResult = LoadModelForImport(importInfo, Handle<Skeleton>::empty);

    return importResult.HasValue() ? importResult.GetValue().skeleton : Handle<Skeleton>::empty;
}

HumanoidMapping MapHipsTo(Name boneName)
{
    HumanoidMapping mapping;
    mapping.SetBoneName(HumanoidBone::Hips, boneName);

    return mapping;
}

String WriteSkeleton(const Handle<Skeleton>& skeleton)
{
    String text;
    ObjectToHMF(Skeleton::StaticClass(), BoxedValue(skeleton), text);

    return text;
}

Handle<Skeleton> ReadSkeleton(const String& text)
{
    HMF::ParseResult parsed = HMF::Parse(text);

    if (!parsed.HasValue() || !parsed.GetValue().Is<Handle<Skeleton>>())
    {
        return Handle<Skeleton>::empty;
    }

    return parsed.GetValue().Get<Handle<Skeleton>>();
}

} // namespace

HYP_ENGINE_TEST(AssetExtension, RoundTripsThroughHMF)
{
    Handle<AssetRegistry> registry = MakeScratchRegistry();
    GlobalContextScope registryScope { AssetRegistryContext { registry } };

    Handle<Skeleton> skeleton = ImportRigSkeleton();
    HYP_TEST_REQUIRE(skeleton.IsValid());
    HYP_TEST_CHECK(!skeleton->GetExtension<HumanoidSkeletonData>().IsValid());

    skeleton->AddExtension(MakeHandle<HumanoidSkeletonData>(MapHipsTo(NAME("Mid"))));

    const String text = WriteSkeleton(skeleton);
    HYP_TEST_CHECK(text.Contains("HumanoidSkeletonData"));
    HYP_TEST_CHECK_MSG(!text.Contains("    Humanoid = "), "a skeleton without legacy humanoid data shouldn't write the Humanoid property");

    Handle<Skeleton> loaded = ReadSkeleton(text);
    HYP_TEST_REQUIRE(loaded.IsValid());

    Handle<HumanoidSkeletonData> data = loaded->GetExtension<HumanoidSkeletonData>();
    HYP_TEST_REQUIRE(data.IsValid());

    HYP_TEST_CHECK(data->GetMapping().GetBoneName(HumanoidBone::Hips) == NAME("Mid"));
    HYP_TEST_CHECK(data->GetOwner().Get() == loaded.Get());
}

HYP_ENGINE_TEST(AssetExtension, KeepsUnregisteredExtensions)
{
    Handle<AssetRegistry> registry = MakeScratchRegistry();
    GlobalContextScope registryScope { AssetRegistryContext { registry } };

    Handle<Skeleton> skeleton = ImportRigSkeleton();
    HYP_TEST_REQUIRE(skeleton.IsValid());

    const String text = WriteSkeleton(skeleton);
    HYP_TEST_REQUIRE_MSG(!text.Contains("Extensions"), "expected no extensions in:\n{}", text);

    const String withUnregistered = InsertProperty(text, "    Extensions = [\n        RetiredPluginSkeletonData {\n            Weight = 2\n        }\n    ]");

    Handle<Skeleton> loaded = ReadSkeleton(withUnregistered);
    HYP_TEST_REQUIRE(loaded.IsValid());
    HYP_TEST_CHECK(loaded->GetExtensionClassNames().Empty());

    const String rewritten = WriteSkeleton(loaded);
    HYP_TEST_CHECK_MSG(rewritten.Contains("RetiredPluginSkeletonData") && rewritten.Contains("Weight = 2"), "the unregistered extension was lost:\n{}", rewritten);
}

HYP_ENGINE_TEST(AssetExtension, MigratesLegacyHumanoid)
{
    Handle<AssetRegistry> registry = MakeScratchRegistry();
    GlobalContextScope registryScope { AssetRegistryContext { registry } };

    Handle<Skeleton> skeleton = ImportRigSkeleton();
    HYP_TEST_REQUIRE(skeleton.IsValid());

    // written the way the old struct-typed Humanoid field was, without a class name
    ToHMFOptions legacyOptions;
    legacyOptions.writeClassName = false;

    String legacyText;
    HYP_TEST_REQUIRE(!BoxedToHMF(BoxedValue(MapHipsTo(NAME("Mid"))), legacyText, nullptr, &legacyOptions).HasError());

    const String text = WriteSkeleton(skeleton);
    HYP_TEST_REQUIRE(!text.Contains("Extensions"));

    const String legacy = InsertProperty(text, String("    Humanoid = ") + legacyText);

    Handle<Skeleton> loaded = ReadSkeleton(legacy);
    HYP_TEST_REQUIRE(loaded.IsValid());
    HYP_TEST_REQUIRE_MSG(loaded->GetLegacyHumanoid().IsValid(), "the legacy Humanoid property wasn't kept:\n{}", legacy);

    HYP_TEST_CHECK(HumanoidSkeletonData::GetSkeletonMapping(*loaded).GetBoneName(HumanoidBone::Hips) == NAME("Mid"));
    HYP_TEST_CHECK(!loaded->GetLegacyHumanoid().IsValid());

    const String rewritten = WriteSkeleton(loaded);
    HYP_TEST_CHECK(rewritten.Contains("HumanoidSkeletonData"));
    HYP_TEST_CHECK_MSG(!rewritten.Contains("    Humanoid = "), "the migrated Humanoid property was written again:\n{}", rewritten);
}

HYP_ENGINE_TEST(AssetExtension, CloneOwnsItsExtensions)
{
    Handle<AssetRegistry> registry = MakeScratchRegistry();
    GlobalContextScope registryScope { AssetRegistryContext { registry } };

    Handle<Skeleton> skeleton = ImportRigSkeleton();
    HYP_TEST_REQUIRE(skeleton.IsValid());

    skeleton->AddExtension(MakeHandle<HumanoidSkeletonData>(MapHipsTo(NAME("Mid"))));

    Handle<AssetObject> clone = skeleton->CloneAsset();
    HYP_TEST_REQUIRE(clone.IsValid() && clone->IsA<Skeleton>());

    Handle<HumanoidSkeletonData> original = skeleton->GetExtension<HumanoidSkeletonData>();
    Handle<HumanoidSkeletonData> cloned = clone->GetExtension<HumanoidSkeletonData>();
    HYP_TEST_REQUIRE(original.IsValid() && cloned.IsValid());

    HYP_TEST_CHECK(cloned != original);
    HYP_TEST_CHECK(cloned->GetOwner().Get() == clone.Get());
    HYP_TEST_CHECK(original->GetOwner().Get() == skeleton.Get());
    HYP_TEST_CHECK(cloned->GetMapping().GetBoneName(HumanoidBone::Hips) == NAME("Mid"));
}

HYP_ENGINE_TEST(AssetExtension, ListsApplicableClasses)
{
    const Array<const Class*> skeletonExtensions = AssetExtension::GetExtensionClasses(Skeleton::StaticClass());

    HYP_TEST_CHECK(skeletonExtensions.Contains(HumanoidSkeletonData::StaticClass()));
    HYP_TEST_CHECK(!AssetExtension::CanExtend(HumanoidSkeletonData::StaticClass(), AssetObject::StaticClass()));
}

} // namespace scene
} // namespace tests
} // namespace Hyperion

#endif // HYP_TESTS
