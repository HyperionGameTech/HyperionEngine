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

#include <Core/CLI/CommandLine.hpp>
#include <Core/Core.hpp>

#include <Core/Utilities/ByteUtil.hpp>
#include <Core/Utilities/GlobalContext.hpp>

#include <Core/IO/ByteWriter.hpp>

#include <Asset/AssetRegistry.hpp>
#include <Asset/AssetObject.hpp>
#include <Asset/BlobStorage.hpp>

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

        }

        return s_definitions;
    }

protected:
    virtual Result Run_Impl(const CommandLineArguments& args) override
    {
        // @TODO Allow Projects/foo dir
        FilePath packageDir = CoreApi::GetBaseDirectory() / "Content" / "Game";
        if (!packageDir.Exists() || !packageDir.IsDirectory())
        {
            return HYP_MAKE_ERROR(Error, "Package path is non existant or is not a directory");
        }
        
        Handle<AssetRegistry> engineRegistry;
        Handle<AssetRegistry> gameRegistry;

        gameRegistry = MakeHandle<AssetRegistry>(AssetRegistryId::Game, packageDir);

        AssetRegistryContext registryContext { gameRegistry };

        Set<AssetRegistry*> registries = { gameRegistry };
        
        if ((engineRegistry = GetEngineAssetRegistry()))
        {
            registries.Add(engineRegistry);
        }

        if (registries.Empty())
        {
            return HYP_MAKE_ERROR(Error, "No valid Engine or Game asset registry found to cook");
        }

        HYP_LOG(Assets, Info, "Cooking blob storage for {} asset registries", registries.Size());

        Result result = Cook(registries);

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

    /*! \brief Collects blob data from every registry into one shared set of per-bucket block
     *  files, so a bucket that exists in both the Engine and Game registries (e.g. Meshes) ends
     *  up in a single block rather than each registry separately overwriting the other's block. */
    static Result Cook(const Set<AssetRegistry*>& registries)
    {
        GlobalContextScope contextScope { CookingContext() };

        EngineGlobals::GetBlobStorage()->Shutdown();

        const FilePath outputContentDir = CoreApi::GetExecutablePath() / "Content";

        Array<TSharedResLock<AssetObject>> readLocks;
        Array<CollectedBlob> collectedBlobs;

        Array<uint64> blockSizes;
        blockSizes.Resize(MaxAssetBuckets);

        for (AssetRegistry* registry : registries)
        {
            registry->LoadAssetDescs();

            for (uint32 bucketIndex = 1; bucketIndex < MaxAssetBuckets; bucketIndex++)
            {
                const AssetBucket& bucket = *AssetBuckets::AllBuckets[bucketIndex];

                Array<AssetDesc> assetDescs;
                registry->GetBucketAssetDescs(bucketIndex, assetDescs);

                if (assetDescs.Empty())
                {
                    continue;
                }

                const FilePath bucketContentDir = outputContentDir / GetAssetBucketName(bucketIndex);

                if (!bucketContentDir.Exists() && !bucketContentDir.MkDir())
                {
                    return HYP_MAKE_ERROR(Error, "Failed to create bucket content directory '{}'", bucketContentDir);
                }

                for (const AssetDesc& assetDesc : assetDescs)
                {
                    Handle<AssetObject> assetObject = registry->GetAsset(bucket, assetDesc.name);

                    if (!assetObject.IsValid())
                    {
                        continue;
                    }

                    readLocks.PushBack(assetObject->GetReadScope());

                    const FilePath manifestPath = bucketContentDir / (String(*assetDesc.name) + ".hmf");

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
                            reference });
                    }
                }
            }
        }

        Array<BlobBlockInfo> blocks;

        for (uint32 bucketIndex = 1; bucketIndex < MaxAssetBuckets; bucketIndex++)
        {
            if (blockSizes[bucketIndex] == 0)
            {
                continue;
            }

            blocks.PushBack(BlobBlockInfo { bucketIndex, blockSizes[bucketIndex] });
        }

        BlobStorage cookedStorage(/* readOnly */ false);
        cookedStorage.Initialize();

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

        return cookedStorage.FinishCook();
    }
};

ENGINE_API const Class* g_clsBlobStorageCookCommandlet = nullptr;

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