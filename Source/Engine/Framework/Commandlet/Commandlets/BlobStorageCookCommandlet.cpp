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
#include <Core/IO/ByteReader.hpp>

#include <Asset/AssetRegistry.hpp>
#include <Asset/AssetObject.hpp>
#include <Asset/AssetBucket.hpp>
#include <Asset/BlobStorage.hpp>

#include <Rendering/Shared.hpp>
#include <Rendering/RenderInterface.hpp>
#include <Rendering/Shader.hpp>
#include <Rendering/Texture.hpp>
#include <Rendering/ShaderManager.hpp>

#include <Rendering/Util/ShaderCompiler.hpp>
#include <Rendering/Util/ShaderPropertyDictionary.hpp>

#include <System/MessageBox.hpp>

#ifdef HYP_EDITOR
#include <Editor/EditorProject.hpp>
#include <Editor/EditorState.hpp>
#endif // HYP_EDITOR

namespace Hyperion {

#ifdef HYP_EDITOR
EDITOR_API HYP_DECLARE_LOG_CHANNEL(Editor);
#endif // HYP_EDITOR

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
                "worlds",
                "",
                "Comma-separated World asset names to cook game content for (all Worlds in the project when empty)",
                CommandLineArgumentFlags::NONE,
                {},
                JSON::Value(""));

            s_definitions.Add(
                "max-texture-size",
                "",
                "Largest width or height a cooked 2D texture keeps; the mips above it are left out (0 keeps every mip)",
                CommandLineArgumentFlags::NONE,
                {},
                JSON::Value(""));

            s_definitions.Add(
                "engine-assets",
                "",
                "File listing the Engine assets to cook, one Bucket/Name per line, as written by a run with --record-engine-assets (every Engine asset when empty)",
                CommandLineArgumentFlags::NONE,
                {},
                JSON::Value(""));

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

            s_definitions.Add(
                "out-engine-content",
                "",
                "Directory to write engine content to, when it shouldn't share the game's content directory",
                CommandLineArgumentFlags::NONE,
                {},
                JSON::Value(""));
        }

        return s_definitions;
    }

protected:
    static FilePath GetDirectory(const String& value, bool mkdirs)
    {
        FilePath dir = (value.StartsWith(".")
                    // Relative path - starts with . (eg "../Foo" or "./Foo")
                    ? (CoreApi::GetBaseDirectory() / value)
                    // Just use provided path.
                    : value);

        // eg "Projects/Foo" when not launched from the repo root
        if (!mkdirs && !dir.IsDirectory() && (CoreApi::GetBaseDirectory() / value).IsDirectory())
        {
            dir = CoreApi::GetBaseDirectory() / value;
        }

        if (!dir.IsDirectory() && (!mkdirs || !dir.MkDir()))
        {
            return FilePath();
        }

        return dir;
    }

    virtual Result Run(const CommandLineArguments& args) override
    {
        if (!g_shaderManager)
        {
            g_shaderManager = new ShaderManager;
        }

        GlobalContextScope contextScope { CookingContext() };

        Handle<AssetRegistry> gameRegistry;

        const String projectArg = args["project"].ToString();
        FilePath projectDir;

#if 0 //def HYP_EDITOR
        // If we're initializing from editor, eg Build>Import Game Content, we wont have `project` arg,
        // instead we will pull from active project
        if (g_editorState.IsValid())
        {
            if (Handle<EditorProject> currentProject = g_editorState->GetCurrentProject(); currentProject.IsValid())
            {
                if (currentProject->IsSaved())
                {
                    // If its already been saved then save the project again first so assets are totally up to date
                    Result saveResult = currentProject->Save();
                    if (saveResult.HasError())
                    {
                        HYP_LOG(Editor, Error, "Failed to save project: {}", saveResult.GetError().GetMessage());
                        return saveResult.GetError();
                    }
                }
                else
                {
                    // Not saved, alert the user that we need them to save the project before this:
                    bool cancel = false;
                    Result saveResult;

                    SystemMessageBox(MessageBoxType::INFO)
                        .Title("Must be saved before cooking game content")
                        .Text("The current project is not yet saved - would you like to save the project to continue with the cook task?")
                        .Button("Save", [currentProject, &saveResult]
                        {
                            saveResult = currentProject->Save();
                            if (saveResult.HasError())
                            {
                                HYP_LOG(Editor, Error, "Failed to save project: {}", saveResult.GetError().GetMessage());

                                SystemMessageBox(MessageBoxType::CRITICAL)
                                            .Title("Project could not be saved")
                                            .Text(String("The project could not be saved: ") + saveResult.GetError().GetMessage()
                                                + "\nThe operation will be aborted to prevent loss of data")
                                            .Button("OK", NoOpFunction<void> {})
                                            .Show();
                                            
                            }
                        })
                        .Button("Discard", NoOpFunction<void> {})
                        .Button("Cancel", [&cancel] { cancel = true; })
                        .Show();

                    if (saveResult.HasError())
                    {
                        return saveResult.GetError();
                    }

                    if (cancel)
                    {
                        return {}; // ok, intentional cancel
                    }
                }

                projectDir = currentProject->GetFilePath().BasePath();
            }
        }
#endif // HYP_EDITOR

        const bool engineOnly = args["engine-only"].ToBool(false);
        if (!engineOnly)
        {
            if (projectArg.Empty())
            {
                return HYP_MAKE_ERROR(Error, "No valid project directory provided (required unless --engine-only is true)");
            }

            if ((projectDir = GetDirectory(projectArg, false)); projectDir.Empty())
            {
                return HYP_MAKE_ERROR(Error, "Package path is non existant or is not a directory: {}", projectArg);
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

        // engine manifests can go to their own directory, for builds that keep engine content apart from the game's (<base>/Content/Engine)
        FilePath outEngineContentDir = outContentDir;

        if (const String outEngineContentArg = args["out-engine-content"].ToString(); outEngineContentArg.Any())
        {
            outEngineContentDir = GetDirectory(outEngineContentArg, true);

            if (outEngineContentDir.Empty())
            {
                return HYP_MAKE_ERROR(Error, "Failed to create engine content directory: {}", outEngineContentArg);
            }
        }

        Array<String> worldNames;

        for (const String& worldName : args["worlds"].ToString().Split(','))
        {
            if (worldName.Trimmed().Any())
            {
                worldNames.PushBack(worldName.Trimmed());
            }
        }

        const String engineAssetListArg = args["engine-assets"].ToString();

        s_maxTextureSize = 0;
        StringUtil::Parse(args["max-texture-size"].ToString(), &s_maxTextureSize);

        Result result = Cook(engineRegistry, gameRegistry, projectDir, worldNames, FilePath(engineAssetListArg), outCacheDir, outContentDir, outEngineContentDir);

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
        size_t size;
        BlobDataReference* reference;
    };

    static inline uint32 s_maxTextureSize = 0;

    // Leaves out the mips above the size limit. The texture keeps the rest of its chain as it was stored.
    static void LimitTextureSize(Texture& texture, uint32 maxTextureSize)
    {
        const TextureDesc textureDesc = texture.GetTextureDesc();

        if (textureDesc.type != TextureType::Texture2D || textureDesc.numLayers != 1 || !textureDesc.HasStoredMips())
        {
            return;
        }

        const uint32 numMips = textureDesc.NumMips();

        Vec3u extent = textureDesc.extent;
        uint32 numMipsRemoved = 0;

        while (MathUtil::Max(extent.x, extent.y) > maxTextureSize && numMipsRemoved + 1 < numMips && textureDesc.mipOffsets[numMipsRemoved] != 0)
        {
            extent = Vec3u(MathUtil::Max(extent.x >> 1, 1u), MathUtil::Max(extent.y >> 1, 1u), 1u);

            ++numMipsRemoved;
        }

        if (numMipsRemoved == 0)
        {
            return;
        }

        const uint32 keptDataStart = textureDesc.mipOffsets[numMipsRemoved - 1];

        ByteBuffer keptData;

        {
            auto readScope = texture.GetReadScope();

            const ConstByteView imageData = texture.GetImageData();

            if (!imageData || imageData.Size() <= keptDataStart)
            {
                return;
            }

            keptData = ByteBuffer(imageData.Slice(keptDataStart));
        }

        TextureDesc limitedDesc = textureDesc;
        limitedDesc.extent = extent;

        for (uint32 mipIndex = 0; mipIndex < TextureDesc::MaxMips; mipIndex++)
        {
            const uint32 sourceIndex = mipIndex + numMipsRemoved;

            limitedDesc.mipOffsets[mipIndex] = (sourceIndex < TextureDesc::MaxMips && textureDesc.mipOffsets[sourceIndex] != 0)
                ? textureDesc.mipOffsets[sourceIndex] - keptDataStart
                : 0;
        }

        auto writeScope = texture.GetWriteScope();

        texture.SetTextureDesc(limitedDesc);
        texture.SetImageData(keptData.ToByteView());
    }

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

        if (s_maxTextureSize != 0 && assetObject->GetPath().GetBucket() == AssetBuckets::Textures)
        {
            LimitTextureSize(*static_cast<Texture*>(assetObject.Get()), s_maxTextureSize);
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

            const size_t headerOffset = ByteUtil::AlignAs(blockSizes[bucketIndex], alignof(BlobHeader));
            blockSizes[bucketIndex] = headerOffset + sizeof(BlobHeader) + reference->size;

            collectedBlobs.PushBack(CollectedBlob {
                bucketIndex,
                StringHash(reference->key),
                tup.GetElement<0>(),
                tup.GetElement<1>(),
                size_t(reference->size),
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

    // Engine assets are referenced from code as well as from content, so what a game needs can't be found by walking
    // references. The list is what a run of the game actually loaded.
    static Result CookListedAssets(
        AssetRegistry* registry,
        const FilePath& assetListPath,
        const FilePath& outputContentDir,
        Array<TSharedResLock<AssetObject>>& readLocks,
        Array<CollectedBlob>& collectedBlobs,
        Array<uint64>& blockSizes)
    {
        FILE* listFile = fopen(assetListPath.Data(), "r");

        if (listFile == nullptr)
        {
            return HYP_MAKE_ERROR(Error, "Failed to open asset list '{}'", assetListPath);
        }

        HYP_DEFER({ fclose(listFile); });

        Set<String> seenEntries;
        Array<Handle<AssetObject>> listedAssets;
        Set<const AssetObject*> listedAssetSet;
        uint32 numMissing = 0;

        char line[1024];

        while (fgets(line, sizeof(line), listFile) != nullptr)
        {
            const String entry = String(line).Trimmed();

            if (entry.Empty() || !seenEntries.Insert(entry).second)
            {
                continue;
            }

            const Array<String> parts = entry.Split('/');

            if (parts.Size() != 2)
            {
                HYP_LOG(Assets, Warning, "Skipping asset list entry '{}', expected Bucket/Name", entry);

                continue;
            }

            const AssetBucket& bucket = GetAssetBucketByName(StringHash(parts[0]));

            if (bucket == AssetBuckets::None)
            {
                HYP_LOG(Assets, Warning, "Skipping asset list entry '{}', no such bucket", entry);

                continue;
            }

            Handle<AssetObject> assetObject = registry->GetAsset(bucket, StringHash(parts[1]));

            if (!assetObject.IsValid())
            {
                HYP_LOG(Assets, Warning, "Listed asset '{}' is not in the registry", entry);

                ++numMissing;

                continue;
            }

            listedAssetSet.Insert(assetObject.Get());
            listedAssets.PushBack(std::move(assetObject));
        }

        for (const Handle<AssetObject>& assetObject : listedAssets)
        {
            // a bundle on disk holds its variants for every backend; its cooked manifest should only name the ones cooked with it
            if (assetObject->GetPath().GetBucket() == AssetBuckets::ShaderBundles)
            {
                ShaderBundle* shaderBundle = static_cast<ShaderBundle*>(assetObject.Get());

                for (auto it = shaderBundle->compiledShaders.Begin(); it != shaderBundle->compiledShaders.End();)
                {
                    if (!listedAssetSet.Contains(static_cast<const AssetObject*>(it->Get())))
                    {
                        it = shaderBundle->compiledShaders.Erase(it);

                        continue;
                    }

                    ++it;
                }
            }

            if (Result result = CookAsset(outputContentDir, assetObject, readLocks, collectedBlobs, blockSizes); result.HasError())
            {
                return result;
            }
        }

        HYP_LOG(Assets, Info, "Cooked {} listed asset(s) from \"{}\", {} missing", listedAssets.Size(), assetListPath, numMissing);

        return {};
    }

    static Result Cook(
        const Handle<AssetRegistry>& engineRegistry, const Handle<AssetRegistry>& gameRegistry,
        const FilePath& projectPath,
        const Array<String>& worldNames,
        const FilePath& engineAssetListPath,
        const FilePath& outputCacheDir, const FilePath& outputContentDir, const FilePath& outputEngineContentDir)
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

            if (engineAssetListPath.Any())
            {
                if (Result result = CookListedAssets(engineRegistry, engineAssetListPath, outputEngineContentDir, readLocks, collectedBlobs, blockSizes); result.HasError())
                {
                    return result;
                }
            }
            else
            {
                for (uint32 bucketIndex = 1; bucketIndex < MaxAssetBuckets; bucketIndex++)
                {
                    if (Result result = CookBucketInFull(engineRegistry, bucketIndex, outputEngineContentDir, readLocks, collectedBlobs, blockSizes); result.HasError())
                    {
                        return result;
                    }
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

                // Loop over all World assets, grab stuff that's reachable from them.
                constexpr AssetBucket WorldsBucket = AssetBuckets::Worlds;

                Array<AssetDesc> assetDescs;
                gameRegistry->GetBucketAssetDescs(WorldsBucket.GetIndex(), assetDescs);

                for (const AssetDesc& assetDesc : assetDescs)
                {
                    if (worldNames.Any() && !worldNames.Contains(String(*assetDesc.name)))
                    {
                        continue;
                    }

                    Handle<AssetObject> worldAsset = gameRegistry->GetAsset(WorldsBucket, assetDesc.name);
                    if (!worldAsset.IsValid())
                    {
                        HYP_LOG(Assets, Warning, "Failed to load World asset \"{}\"", assetDesc.name);
                        continue;
                    }

                    AssetRegistry::WalkAssetDeep(BoxedValue(worldAsset), callback);
                }

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
            if (collectedBlob.reference->raw == nullptr || collectedBlob.reference->size != collectedBlob.size)
            {
                return HYP_MAKE_ERROR(Error, "Blob data for key {} became invalid between collection and write (asset was unpaged or reallocated during cooking)", collectedBlob.key.GetHashCode().Value());
            }

            BlobHeader header {};

            const size_t magicLength = collectedBlob.magic ? std::strlen(collectedBlob.magic) : 0;
            Memory::Copy((char*)header.magic, collectedBlob.magic, MathUtil::Min(magicLength, sizeof(header.magic)));
            header.payloadOffset = 0;
            header.payloadSize = collectedBlob.size;
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

        // Write shader cache here as well
        HYP_LOG(Assets, Info, "Writing shader cache data...");
        g_shaderManager->WriteShaderCache(outputCacheDir);
        HYP_LOG(Assets, Info, "Shader cache data written.");

        // For webGPU, collecting minimal pipelines
        const FilePath pipelineManifestPath = EngineGlobals::GetCacheDirectory() / "pipelinevariants.bin";

        if (pipelineManifestPath.Exists())
        {
            FileByteReader pipelineManifestReader { pipelineManifestPath };
            const ByteBuffer pipelineManifest = pipelineManifestReader.Read();
            pipelineManifestReader.Close();

            FileByteWriter pipelineManifestWriter { outputCacheDir / "pipelinevariants.bin" };
            pipelineManifestWriter.Write(pipelineManifest.Data(), pipelineManifest.Size());
            pipelineManifestWriter.Close();
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
