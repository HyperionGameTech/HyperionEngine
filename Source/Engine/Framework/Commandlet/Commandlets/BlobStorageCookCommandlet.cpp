/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#include <HyperionPch.hpp>

#include <Framework/Commandlet/Commandlet.hpp>
#include <Framework/EngineGlobals.hpp>

#include <Core/Reflection/ClassUtils.hpp>
#include <Core/Reflection/ClassRegistry.hpp>
#include <Core/Reflection/BoxedValue.hpp>

#include <Core/CLI/CommandLine.hpp>
#include <Core/Core.hpp>

#include <Core/Utilities/ByteUtil.hpp>
#include <Core/Utilities/GlobalContext.hpp>

#include <Core/Containers/Set.hpp>

#include <Core/IO/ByteWriter.hpp>

#include <Asset/AssetRegistry.hpp>
#include <Asset/AssetObject.hpp>
#include <Asset/AssetBucket.hpp>
#include <Asset/BlobStorage.hpp>

#include <Rendering/Shared.hpp>

#include <Rendering/Util/ShaderPropertyDictionary.hpp>

#ifdef HYP_EDITOR
#include <Editor/EditorProject.hpp>
#endif // HYP_EDITOR

namespace Hyperion {

struct CookingContext {};

class BlobStorageCookCommandlet final : public CommandletBase
{
    HYP_OBJECT_BODY(BlobStorageCookCommandlet);

public:
    virtual ~BlobStorageCookCommandlet() override = default;

    HYP_METHOD()
    static const CommandLineArgumentDefinitions& GetArgumentDefinitions()
    {
        static CommandLineArgumentDefinitions s_definitions;

        static bool s_initialized = false;
        if (!s_initialized)
        {
            s_initialized = true;

            s_definitions.Add(
                "project",
                "c",
                "Project to content to cook for",
                CommandLineArgumentFlags::NONE,
                {},
                JSON::Value(""));

            s_definitions.Add(
                "engine-only",
                "",
                "If true, only engine content will be cooked (minimal needed cache/content to start a game)",
                CommandLineArgumentFlags::NONE,
                {},
                false);
            
            s_definitions.Add(
                "out-cache",
                "c",
                "Directory to write cache to",
                CommandLineArgumentFlags::REQUIRED,
                {},
                JSON::Value(""));
            
            s_definitions.Add(
                "out-content",
                "c",
                "Directory to write content to",
                CommandLineArgumentFlags::REQUIRED,
                {},
                JSON::Value(""));
        }

        return s_definitions;
    }

protected:
    static FilePath GetDirectory(const String& value, bool mkdirs)
    {
        const FilePath dir = (value.StartsWith(".")
                    // Relative path - starts with . (eg "../Foo" or "./Foo")
                    ? (CoreApi::GetBaseDirectory() / value)
                    // Just use provided path.
                    : value);

        if (!dir.IsDirectory() && (mkdirs && !dir.MkDir()))
        {
            return FilePath();
        }

        return dir;
    }

    virtual Result Run(const CommandLineArguments& args) override
    {
        GlobalContextScope contextScope { CookingContext() };
        
        Handle<AssetRegistry> gameRegistry;

        const String projectArg = args["project"].ToString();
        FilePath projectDir;

        const bool engineOnly = args["engine-only"].ToBool(false);
        if (!engineOnly)
        {
            if (projectArg.Empty())
            {
                return HYP_MAKE_ERROR(Error, "No valid project directory provided (required unless --engine-only is true)");
            }

            if ((projectDir = GetDirectory(projectArg, false)); projectDir.Empty())
            {
                return HYP_MAKE_ERROR(Error, "Package path is non existant or is not a directory: {}", projectDir);
            }
            
            gameRegistry = MakeHandle<AssetRegistry>(AssetRegistryId::Game, projectDir);
            gameRegistry->Initialize(nullptr);
        }

        Handle<AssetRegistry> engineRegistry;
        engineRegistry = GetEngineAssetRegistry();

        if (!engineRegistry.IsValid())
        {
            engineRegistry = MakeHandle<AssetRegistry>(
                AssetRegistryId::Engine,
                EngineGlobals::GetContentDirectory<HYP_STATIC_STRING("Engine")>());

            engineRegistry->Initialize(nullptr);
        }

        HYP_LOG(Assets, Info, "Cooking blob storage");

        const FilePath outCacheDir = GetDirectory(args["out-cache"].ToString(), true);
        if (outCacheDir.Empty())
        {
            return HYP_MAKE_ERROR(Error, "Failed to create cache directory: {}", args["out-cache"].ToString());
        }

        const FilePath outContentDir = GetDirectory(args["out-content"].ToString(), true);
        if (outContentDir.Empty())
        {
            return HYP_MAKE_ERROR(Error, "Failed to create content directory: {}", args["out-content"].ToString());
        }

        Result result = Cook(engineRegistry, gameRegistry, projectDir, outCacheDir, outContentDir);

        if (result.HasError())
        {
            HYP_LOG(Assets, Error, "Failed to cook blob storage: {}", result.GetError().GetMessage());

            return result;
        }

        HYP_LOG(Assets, Info, "Blob storage cook complete.");

        return {};
    }

private:
      struct CollectedBlob
    {
        uint32 bucketIndex;
        StringHash key;
        const char* magic;
        uint16 version;
        BlobDataReference* reference;
    };

    static Result CookAsset(
        const FilePath& outputContentDir,
        const Handle<AssetObject>& assetObject,
        Array<TSharedResLock<AssetObject>>& readLocks,
        Array<CollectedBlob>& collectedBlobs,
        Array<uint64>& blockSizes)
    {
        if (!assetObject.IsValid())
        {
            return {};
        }

        if (assetObject->IsTransient())
        {
            return {};
        }

        const uint32 bucketIndex = assetObject->GetPath().GetBucket().GetIndex();
        const Name assetName = assetObject->GetPath().GetName();

        const FilePath bucketContentDir = outputContentDir / GetAssetBucketName(bucketIndex);

        if (!bucketContentDir.Exists() && !bucketContentDir.MkDir())
        {
            return HYP_MAKE_ERROR(Error, "Failed to create bucket content directory '{}'", bucketContentDir);
        }

        readLocks.PushBack(assetObject->GetReadScope());

        const FilePath manifestPath = bucketContentDir / (String(*assetName) + ".hmf");

        FileByteWriter manifestWriter { manifestPath };

        if (!manifestWriter.IsOpen())
        {
            return HYP_MAKE_ERROR(Error, "Failed to open manifest file '{}' for writing", manifestPath);
        }

        if (Result manifestResult = assetObject->SaveManifest(manifestWriter); manifestResult.HasError())
        {
            return manifestResult;
        }

        manifestWriter.Close();

        Array<Tuple<const char*, uint16, BlobDataReference*>> blobDataReferences;
        assetObject->CollectBlobDataReferences(blobDataReferences);

        for (auto& tup : blobDataReferences)
        {
            BlobDataReference* reference = tup.GetElement<2>();

            if (!reference->raw || reference->size == 0)
            {
                continue;
            }

            // Mirrors the header-offset alignment BlobStorage::PutData performs when it
            // actually writes the blob, so the size computed here matches exactly.
            const size_t headerOffset = ByteUtil::AlignAs(blockSizes[bucketIndex], alignof(BlobHeader));
            blockSizes[bucketIndex] = headerOffset + sizeof(BlobHeader) + reference->size;

            collectedBlobs.PushBack(CollectedBlob {
                bucketIndex,
                StringHash(reference->key),
                tup.GetElement<0>(),
                tup.GetElement<1>(),
                reference
            });
        }

        return {};
    }

    static Result CookBucketInFull(
        AssetRegistry* registry,
        uint32 bucketIndex,
        const FilePath& outputContentDir,
        Array<TSharedResLock<AssetObject>>& readLocks,
        Array<CollectedBlob>& collectedBlobs,
        Array<uint64>& blockSizes)
    {
        const AssetBucket& bucket = *AssetBuckets::AllBuckets[bucketIndex];

        Array<AssetDesc> assetDescs;
        registry->GetBucketAssetDescs(bucketIndex, assetDescs);

        for (const AssetDesc& assetDesc : assetDescs)
        {
            Handle<AssetObject> assetObject = registry->GetAsset(bucket, assetDesc.name);

            if (Result result = CookAsset(outputContentDir, assetObject, readLocks, collectedBlobs, blockSizes); result.HasError())
            {
                return result;
            }
        }

        return {};
    }

    static Result Cook(
        const Handle<AssetRegistry>& engineRegistry, const Handle<AssetRegistry>& gameRegistry,
        const FilePath& projectPath,
        const FilePath& outputCacheDir, const FilePath& outputContentDir)
    {
        Array<TSharedResLock<AssetObject>> readLocks;
        Array<CollectedBlob> collectedBlobs;

        Array<uint64> blockSizes;
        blockSizes.Resize(MaxAssetBuckets);

        // Engine content, cook everything in the registry
        if (engineRegistry)
        {
            SetEngineAssetRegistry(engineRegistry);

            GlobalContextScope assetRegistryContextScope { AssetRegistryContext { engineRegistry } };
            engineRegistry->LoadAssetDescs();

            for (uint32 bucketIndex = 1; bucketIndex < MaxAssetBuckets; bucketIndex++)
            {
                if (Result result = CookBucketInFull(engineRegistry, bucketIndex, outputContentDir, readLocks, collectedBlobs, blockSizes); result.HasError())
                {
                    return result;
                }
            }
        }

#ifdef HYP_EDITOR
        // Game content: only cook what's actually reachable from the project's own data to strip assets that are unused.
        if (gameRegistry)
        {
            GlobalContextScope assetRegistryContextScope { AssetRegistryContext { gameRegistry } };
            gameRegistry->LoadAssetDescs();

            Array<Handle<AssetObject>> assetsToCook;

            TResult<Handle<EditorProject>> loadProjectResult = EditorProject::Load(projectPath);

            if (loadProjectResult.HasValue())
            {
                Set<AssetObject*> seenAssets;

                auto callback = [&assetsToCook, &seenAssets](const Handle<AssetObject>& assetObject)
                    {
                        if (!assetObject.IsValid())
                        {
                            return;
                        }

                        if (!seenAssets.Insert(assetObject.Get()).second)
                        {
                            return;
                        }

                        assetsToCook.PushBack(assetObject);
                    };

                AssetRegistry::WalkAssetDeep(BoxedValue(loadProjectResult.GetValue()), callback);

                HYP_LOG(Assets, Info, "Found {} asset(s) reachable from project at \"{}\"", assetsToCook.Size(), projectPath);
            }
            else
            {
                HYP_LOG(Assets, Warning, "Failed to load EditorProject from \"{}\" ({}), falling back to cooking every asset in the registry",
                    projectPath, loadProjectResult.GetError().GetMessage());

                for (uint32 bucketIndex = 1; bucketIndex < MaxAssetBuckets; bucketIndex++)
                {
                    Array<AssetDesc> assetDescs;
                    gameRegistry->GetBucketAssetDescs(bucketIndex, assetDescs);

                    const AssetBucket& bucket = *AssetBuckets::AllBuckets[bucketIndex];

                    for (const AssetDesc& assetDesc : assetDescs)
                    {
                        assetsToCook.PushBack(gameRegistry->GetAsset(bucket, assetDesc.name));
                    }
                }
            }

            for (const Handle<AssetObject>& assetObject : assetsToCook)
            {
                if (Result result = CookAsset(outputContentDir, assetObject, readLocks, collectedBlobs, blockSizes); result.HasError())
                {
                    return result;
                }
            }
        }
#endif // HYP_EDITOR

        Array<BlobBlockInfo> blocks;

        for (uint32 bucketIndex = 1; bucketIndex < MaxAssetBuckets; bucketIndex++)
        {
            if (blockSizes[bucketIndex] == 0)
            {
                continue;
            }

            blocks.PushBack(BlobBlockInfo { bucketIndex, blockSizes[bucketIndex] });
        }

        BlobStorage cookedStorage;
        cookedStorage.Lock(outputCacheDir, /* readOnly */ false);

        bool locked = true;
        HYP_DEFER({
            if (locked)
            {
                cookedStorage.Unlock();
            }
        });

        if (Result result = cookedStorage.BeginCook(blocks); result.HasError())
        {
            return result;
        }

        for (const CollectedBlob& collectedBlob : collectedBlobs)
        {
            BlobHeader header {};

            const size_t magicLength = collectedBlob.magic ? std::strlen(collectedBlob.magic) : 0;
            Memory::Copy((char*)header.magic, collectedBlob.magic, MathUtil::Min(magicLength, sizeof(header.magic)));
            header.payloadOffset = 0;
            header.payloadSize = collectedBlob.reference->size;
            header.version = collectedBlob.version;

            if (!cookedStorage.PutData(collectedBlob.bucketIndex, collectedBlob.key, header, collectedBlob.reference->raw))
            {
                return HYP_MAKE_ERROR(Error, "Failed to write cooked blob data for key {}", collectedBlob.key.GetHashCode().Value());
            }
        }

        if (Result result = cookedStorage.FinishCook(); result.HasError())
        {
            return result;
        }

        cookedStorage.Unlock();
        locked = false;

        // Write the shader property dictionary so the runtime can resolve
        // ShaderProperty names to their interned IDs (used by ShaderPropertySet).
        {
            const FilePath shaderPropertyDbPath = outputCacheDir / "shaderprops.bin";

            FileByteWriter writer { shaderPropertyDbPath };
            WriteShaderPropertyDictionary(writer);
            writer.Close();
        }

        return {};
    }
};

HYP_EXPORT const Class* g_clsBlobStorageCookCommandlet = nullptr;

const Class* BlobStorageCookCommandlet::StaticClass()
{
    return g_clsBlobStorageCookCommandlet;
}

// clang-format off

HYP_BEGIN_CLASS(BlobStorageCookCommandlet, -1, 0, NAME("CommandletBase"), ClassAttribute("command", "blobstoragecook"))
    Method(NAME("GetArgumentDefinitions"), &Type::GetArgumentDefinitions)
HYP_END_CLASS

// clang-format on

HYP_REGISTER_STATIC_CLASS(BlobStorageCookCommandlet);

} // namespace Hyperion
