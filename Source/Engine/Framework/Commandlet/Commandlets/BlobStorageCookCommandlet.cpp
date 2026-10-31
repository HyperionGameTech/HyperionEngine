/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#include <HyperionPch.hpp>

#include <Framework/Commandlet/Commandlet.hpp>
#include <Framework/EngineGlobals.hpp>
#include <Framework/EngineDefaults.hpp>
#include <Framework/CVarManager.hpp>

#include <Core/Reflection/ClassUtils.hpp>
#include <Core/Reflection/ClassRegistry.hpp>
#include <Core/Reflection/BoxedValue.hpp>

#include <Core/CLI/CommandLine.hpp>
#include <Core/Core.hpp>

#include <Core/Utilities/ByteUtil.hpp>
#include <Core/Utilities/GlobalContext.hpp>
#include <Core/Utilities/StringUtil.hpp>

#include <Core/Containers/Set.hpp>

#include <Core/Threading/TaskSystem.hpp>

#include <Core/IO/ByteWriter.hpp>
#include <Core/IO/ByteReader.hpp>

#include <Core/DataProcessing/HMF/HMF.hpp>
#include <Core/DataProcessing/Shared/CompilerError.hpp>
#include <Core/DataProcessing/Shared/ErrorList.hpp>
#include <Core/DataProcessing/JSON/JSON.hpp>

#include <Asset/AssetRegistry.hpp>
#include <Asset/AssetObject.hpp>
#include <Asset/AssetReference.hpp>
#include <Asset/AssetBucket.hpp>
#include <Asset/BlobStorage.hpp>

#include <Plugins/PluginManager.hpp>

#include <Rendering/Shared.hpp>
#include <Rendering/RenderInterface.hpp>
#include <Rendering/Shader.hpp>
#include <Rendering/Texture.hpp>
#include <Rendering/Mesh.hpp>
#include <Rendering/ShaderManager.hpp>

#include <Rendering/Util/ShaderCompiler.hpp>
#include <Rendering/Util/ShaderPropertyDictionary.hpp>

#include <System/MessageBox.hpp>

#ifdef HYP_EDITOR

#include <Scene/World.hpp>

#include <Scene/Scene.hpp>
#include <Scene/Node.hpp>
#include <Scene/EnvProbe.hpp>
#include <Scene/Light/Light.hpp>

#include <Scene/WorldGrid/WorldGrid.hpp>
#include <Scene/WorldGrid/WorldGridLayer.hpp>
#include <Scene/WorldGrid/Terrain/TerrainWorldGridLayer.hpp>

#include <Rendering/Material.hpp>
#include <Rendering/RendererMain.hpp>
#include <Rendering/RenderHelpers.hpp>

#include <Editor/EditorProject.hpp>
#include <Editor/EditorProjectLayout.hpp>
#include <Editor/EditorState.hpp>
#include <Editor/EditorPackagingSettings.hpp>

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
                "plugins",
                "",
                "Comma-separated built-in plugins the project enables, whose content is cooked with the engine's",
                CommandLineArgumentFlags::NONE,
                {},
                JSON::Value(""));

            s_definitions.Add(
                "max-texture-size",
                "",
                "Largest texture dimension in the cooked game, textures larger than this will be downscaled. Use 0 for no limit",
                CommandLineArgumentFlags::NONE,
                {},
                JSON::Value(""));

            s_definitions.Add(
                "target-platform",
                "",
                "Platform whose packaging settings in the project apply (windows, mac, linux, android, ios, web)",
                CommandLineArgumentFlags::NONE,
                {},
                JSON::Value(""));

            s_definitions.Add(
                "engine-assets",
                "",
                "A file listing Engine assets to cook besides what the project reaches, one Bucket/Name per line",
                CommandLineArgumentFlags::NONE,
                {},
                JSON::Value(""));

            s_definitions.Add(
                "shader-target",
                "",
                "Compiles and cooks shaders for a target. Possible values: `web`, `android`, or `host` to use the active platform and graphics backend.",
                CommandLineArgumentFlags::NONE,
                {},
                JSON::Value(""));

            s_definitions.Add(
                "target-config",
                "",
                "The target's EngineConfig json",
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

#ifdef HYP_EDITOR
            TResult<FilePath> contentDirResult = ResolveProjectContentDirectory(projectDir);

            if (contentDirResult.HasError())
            {
                return contentDirResult.GetError();
            }

            const FilePath contentDir = contentDirResult.GetValue();
#else
            const FilePath contentDir = projectDir;
#endif // HYP_EDITOR

            gameRegistry = MakeHandle<AssetRegistry>(AssetRegistryId::Game, contentDir);
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

        // the editor cooking in-process has its project's plugins mounted already
        if (const String pluginsArgument = args["plugins"].ToString(); pluginsArgument.Any())
        {
            Array<StringHash> pluginNames;
            Array<Name> pluginNameHandles;

            for (const String& pluginName : pluginsArgument.Split(','))
            {
                if (pluginName.Any())
                {
                    pluginNames.PushBack(StringHash(pluginName.ToAnsi()));
                    pluginNameHandles.PushBack(CreateNameFromDynamicString(pluginName.ToAnsi()));
                }
            }

            s_pluginContentDirectories = PluginManager::GetInstance().GetPluginContentDirectories(pluginNames);

            PluginManager::GetInstance().PreloadPlugins(pluginNameHandles);

            for (const FilePath& contentDirectory : s_pluginContentDirectories)
            {
                engineRegistry->AddContentRoot(contentDirectory);
            }
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

        s_hasMaxTextureSizeArgument = args["max-texture-size"].ToString().Any();
#ifdef HYP_EDITOR
        s_hasTargetPlatform = ParseTargetPlatform(args["target-platform"].ToString().ToAnsi(), s_targetPlatform);
#endif // HYP_EDITOR

        s_shaderTarget = HostShaderCompileTarget;

#ifdef HYP_EDITOR
        if (s_hasTargetPlatform && s_targetPlatform == TargetPlatform::Android)
        {
            s_shaderTarget = { ShaderCompileTargetPlatform::Android, ShaderCompileTargetBackend::Vulkan };
        }
        else if (s_hasTargetPlatform && s_targetPlatform == TargetPlatform::Web)
        {
            s_shaderTarget = { ShaderCompileTargetPlatform::Web, ShaderCompileTargetBackend::WebGPU };
        }
#endif // HYP_EDITOR

        ShaderTarget shaderTarget;
        Array<Handle<AssetObject>> targetShaderAssets;

        if (const String shaderTargetArg = args["shader-target"].ToString(); shaderTargetArg.Any())
        {
            SetEngineAssetRegistry(engineRegistry);

            if (Result compileResult = CompileTargetShaders(engineRegistry, shaderTargetArg, FilePath(args["target-config"].ToString()), shaderTarget, targetShaderAssets); compileResult.HasError())
            {
                return compileResult;
            }
        }

        Result result = Cook(engineRegistry, gameRegistry, projectDir, worldNames, FilePath(engineAssetListArg), shaderTarget, std::move(targetShaderAssets), outCacheDir, outContentDir, outEngineContentDir);

        if (result.HasError())
        {
            HYP_LOG(Assets, Error, "Failed to cook blob storage: {}", result.GetError().GetMessage());

            return result;
        }

        HYP_LOG(Assets, Info, "Blob storage cook complete.");

        return {};
    }

private:
    static constexpr size_t InvalidBlobIndex = size_t(-1);

      struct CollectedBlob
    {
        uint32 bucketIndex;
        StringHash key;
        const char* magic;
        uint16 version;
        size_t size;
        BlobDataReference* reference;
        size_t aliasOf = InvalidBlobIndex;
    };

    static inline uint32 s_maxTextureSize = 0;
    static inline bool s_hasMaxTextureSizeArgument = false;
#ifdef HYP_EDITOR
    static inline bool s_hasTargetPlatform = false;
    static inline TargetPlatform s_targetPlatform = TargetPlatform::Windows;
#endif // HYP_EDITOR
    static inline ShaderCompileTarget s_shaderTarget;
    static inline Set<const AssetObject*> s_dataOnlyTextures;

    static inline Array<FilePath> s_pluginContentDirectories;

    // all of the enabled plugins' content: their code loads assets by path, where no world shows them
    static void AddPluginContent(const Handle<AssetRegistry>& engineRegistry, const ProcRef<void(const Handle<AssetObject>&)>& onAssetFound)
    {
        if (!engineRegistry.IsValid())
        {
            return;
        }

        GlobalContextScope engineContextScope { AssetRegistryContext { engineRegistry } };

        for (const FilePath& contentDirectory : s_pluginContentDirectories)
        {
            uint32 numAssets = 0;

            for (const AssetBucket* bucket : AssetBuckets::AllBuckets)
            {
                const FilePath bucketDirectory = contentDirectory / bucket->GetName();

                if (bucket == &AssetBuckets::None || !bucketDirectory.IsDirectory())
                {
                    continue;
                }

                for (auto iter = bucketDirectory.OpenDirectory(); iter.HasNext(); iter.Advance())
                {
                    const FilePath current = iter.Current();

                    if (iter.CurrentIsDirectory() || current.GetExtension() != "hmf")
                    {
                        continue;
                    }

                    const Handle<AssetObject> asset = engineRegistry->GetAsset(*bucket, StringHash(StringUtil::StripExtension(current.Basename())));

                    if (!asset.IsValid())
                    {
                        HYP_LOG(Assets, Warning, "Plugin asset '{}' could not be loaded for the cook", current);

                        continue;
                    }

                    AssetRegistry::WalkAssetDeep(BoxedValue(asset), onAssetFound);
                    ++numAssets;
                }
            }

            HYP_LOG(Assets, Info, "Cooking {} asset(s) of plugin content '{}'", numAssets, contentDirectory);
        }
    }

#ifdef HYP_EDITOR
    static bool IsDataTextureSlot(uint32 slotIndex)
    {
        const MaterialTextureKey key = MaterialTextureKey(1ull << slotIndex);

        return key == MaterialTextureKey::Metalness || key == MaterialTextureKey::Roughness || key == MaterialTextureKey::AmbientOcclusion;
    }

    // the engine assets engine code uses whatever the content is; the terrain ones only for a project with terrain
    static void AddEngineDefaults(const Handle<AssetRegistry>& engineRegistry, bool usesTerrain, const ProcRef<void(const Handle<AssetObject>&)>& onAssetFound)
    {
        if (!engineRegistry.IsValid())
        {
            return;
        }

        GlobalContextScope engineContextScope { AssetRegistryContext { engineRegistry } };

        EngineDefaults engineDefaults = EngineDefaults::Get();

        if (!usesTerrain)
        {
            engineDefaults.terrain = EngineTerrainDefaults {};
        }

        AssetRegistry::WalkAssetDeep(BoxedValue(engineDefaults), onAssetFound);
    }

    static void FindDataOnlyTextures(const Array<Handle<AssetObject>>& assetsToCook)
    {
        Set<const AssetObject*> dataTextures;
        Set<const AssetObject*> detailTextures;

        for (const Handle<AssetObject>& assetObject : assetsToCook)
        {
            const Handle<Material>& material = TryCast<Material>(assetObject);

            if (!material.IsValid())
            {
                continue;
            }

            for (uint32 slotIndex = 0; slotIndex < MaterialTextures::MaxTextures; slotIndex++)
            {
                const Handle<Texture>& texture = material->GetTextureAtIndex(slotIndex);

                if (!texture.IsValid())
                {
                    continue;
                }

                if (IsDataTextureSlot(slotIndex))
                {
                    dataTextures.Insert(texture.Get());
                }
                else
                {
                    detailTextures.Insert(texture.Get());
                }
            }
        }

        s_dataOnlyTextures.Clear();

        for (const AssetObject* texture : dataTextures)
        {
            if (!detailTextures.Contains(texture))
            {
                s_dataOnlyTextures.Insert(texture);
            }
        }
    }
#endif // HYP_EDITOR

    static bool HaveSameContent(const CollectedBlob& a, const CollectedBlob& b)
    {
        if (a.bucketIndex != b.bucketIndex || a.size != b.size || a.version != b.version)
        {
            return false;
        }

        if ((a.magic == nullptr) != (b.magic == nullptr) || (a.magic != nullptr && std::strcmp(a.magic, b.magic) != 0))
        {
            return false;
        }

        return Memory::Compare(static_cast<const ubyte*>(a.reference->raw), static_cast<const ubyte*>(b.reference->raw), a.size) == 0;
    }

    static void DeduplicateBlobs(Array<CollectedBlob>& collectedBlobs, Array<uint64>& blockSizes)
    {
        Map<uint64, Array<size_t>> blobsByContent;

        size_t numDuplicates = 0;
        uint64 numBytesSaved = 0;

        for (uint64& blockSize : blockSizes)
        {
            blockSize = 0;
        }

        for (size_t blobIndex = 0; blobIndex < collectedBlobs.Size(); blobIndex++)
        {
            CollectedBlob& blob = collectedBlobs[blobIndex];

            const uint64 contentHash = FNV1::DoHashWords(blob.reference->raw, blob.size, (uint64(blob.bucketIndex) << 32) ^ blob.size);

            Array<size_t>& candidates = blobsByContent[contentHash];

            for (const size_t candidateIndex : candidates)
            {
                if (HaveSameContent(collectedBlobs[candidateIndex], blob))
                {
                    blob.aliasOf = candidateIndex;

                    break;
                }
            }

            if (blob.aliasOf != InvalidBlobIndex)
            {
                numDuplicates++;
                numBytesSaved += blob.size;

                continue;
            }

            candidates.PushBack(blobIndex);

            const size_t headerOffset = ByteUtil::AlignAs(blockSizes[blob.bucketIndex], alignof(BlobHeader));
            blockSizes[blob.bucketIndex] = headerOffset + sizeof(BlobHeader) + blob.size;
        }

        if (numDuplicates != 0)
        {
            HYP_LOG(Assets, Info, "Stored {} duplicate blob(s) once, saving {} KiB", numDuplicates, numBytesSaved / 1024);
        }
    }

    // Leaves out the mips above the size limit. The texture keeps the rest of its chain as it was stored.
    static void LimitTextureSize(Texture& texture, uint32 maxTextureSize)
    {
        TextureDesc textureDesc = texture.GetTextureDesc();

        if (textureDesc.type != TextureType::Texture2D || textureDesc.numLayers != 1 || !textureDesc.HasStoredMips())
        {
            return;
        }

        if (MathUtil::Max(textureDesc.extent.x, textureDesc.extent.y) <= maxTextureSize)
        {
            return;
        }

        ByteBuffer imageData;

        {
            auto readScope = texture.GetReadScope();

            const ConstByteView storedData = texture.GetImageData();

            if (!storedData)
            {
                return;
            }

            imageData = ByteBuffer(storedData);
        }

        if (!Texture::DropMipsAbove(maxTextureSize, textureDesc, imageData))
        {
            return;
        }

        auto writeScope = texture.GetWriteScope();

        texture.SetTextureDesc(textureDesc);
        texture.SetImageData(imageData.ToByteView());
    }

    static Result CookAsset(
        const FilePath& outputContentDir,
        const Handle<AssetObject>& assetObject,
        Array<TSharedResLock<AssetObject>>& readLocks,
        Array<CollectedBlob>& collectedBlobs,
        Array<uint64>& blockSizes,
        Set<const AssetObject*>& cookedAssets)
    {
        if (!assetObject.IsValid())
        {
            return {};
        }

        if (assetObject->IsTransient())
        {
            return {};
        }

        if (!cookedAssets.Insert(assetObject.Get()).second)
        {
            return {};
        }

        if (s_maxTextureSize != 0 && assetObject->GetPath().GetBucket() == AssetBuckets::Textures)
        {
            const bool isDataOnly = s_dataOnlyTextures.Contains(assetObject.Get());

            LimitTextureSize(*static_cast<Texture*>(assetObject.Get()), isDataOnly ? MathUtil::Max(s_maxTextureSize / 2, 1u) : s_maxTextureSize);
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

            if (reference->size == 0)
            {
                continue;
            }

            if (!reference->raw)
            {
                HYP_LOG(Assets, Warning, "Blob data '{}' of {} could not be paged in and is left out of the cook; delete its manifest to have it rebuilt", tup.GetElement<0>(), assetObject->GetPath());

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

    static bool IsShaderForThisBuild(const Shader& shader)
    {
        // a variant without TARGET or BACKEND isn't specific to another build
        return GetShaderCompileTargetForProperties(shader.properties, s_shaderTarget) == s_shaderTarget;
    }

    static Result CookBucketInFull(
        AssetRegistry* registry,
        uint32 bucketIndex,
        const FilePath& outputContentDir,
        Array<TSharedResLock<AssetObject>>& readLocks,
        Array<CollectedBlob>& collectedBlobs,
        Array<uint64>& blockSizes,
        Set<const AssetObject*>& cookedAssets)
    {
        const AssetBucket& bucket = *AssetBuckets::AllBuckets[bucketIndex];

        Array<AssetDesc> assetDescs;
        registry->GetBucketAssetDescs(bucketIndex, assetDescs);

        uint32 numForeignShaders = 0;

        for (const AssetDesc& assetDesc : assetDescs)
        {
            if (bucket == AssetBuckets::ShaderBundles && !IsShaderBundleForTarget(assetDesc.name, s_shaderTarget))
            {
                continue;
            }

            Handle<AssetObject> assetObject = registry->GetAsset(bucket, assetDesc.name);

            if (bucket == AssetBuckets::Shaders && assetObject.IsValid() && !IsShaderForThisBuild(*static_cast<const Shader*>(assetObject.Get())))
            {
                ++numForeignShaders;

                continue;
            }

            if (bucket == AssetBuckets::ShaderBundles && assetObject.IsValid())
            {
                ShaderBundle* shaderBundle = static_cast<ShaderBundle*>(assetObject.Get());

                Array<Handle<Shader>> allShaders = shaderBundle->compiledShaders;

                for (auto it = shaderBundle->compiledShaders.Begin(); it != shaderBundle->compiledShaders.End();)
                {
                    if (it->IsValid() && !IsShaderForThisBuild(**it))
                    {
                        it = shaderBundle->compiledShaders.Erase(it);

                        continue;
                    }

                    ++it;
                }

                Result result = CookAsset(outputContentDir, assetObject, readLocks, collectedBlobs, blockSizes, cookedAssets);

                shaderBundle->compiledShaders = std::move(allShaders);

                if (result.HasError())
                {
                    return result;
                }

                continue;
            }

            if (Result result = CookAsset(outputContentDir, assetObject, readLocks, collectedBlobs, blockSizes, cookedAssets); result.HasError())
            {
                return result;
            }
        }

        if (numForeignShaders != 0)
        {
            HYP_LOG(Assets, Info, "Left out {} shader variant(s) compiled for other backends or targets", numForeignShaders);
        }

        return {};
    }

    struct ShaderTarget
    {
        bool isEnabled = false;
        String name;
        ShaderCompileTarget compileTarget;
        JSON::Value config;
        Set<Name> materialShaderNames;
    };

    static bool GetTargetBool(
        const JSON::Value& targetConfig,
        const String& name,
        bool defaultValue)
    {
        if (name.Empty())
        {
            return defaultValue;
        }

        if (targetConfig.IsObject())
        {
            const auto value = targetConfig.Get(name);

            if (value.IsObject())
            {
                if (const auto enabled = value["Enabled"]; !enabled.IsNullOrUndefined())
                {
                    return enabled.ToBool();
                }
            }
            else if (!value.IsNullOrUndefined())
            {
                return value.ToBool();
            }
        }

        if (const CVarBase* cvar = CVarManager::GetInstance().FindVar(ANSIString(name.Data())); cvar != nullptr && cvar->GetDefaultValue().Is<bool>())
        {
            return cvar->GetDefaultValue().Get<bool>();
        }

        return defaultValue;
    }

    static bool CheckCondition(const JSON::Value& targetConfig, const String& condition)
    {
        return GetTargetBool(targetConfig, condition, true);
    }

    static Result CompileTargetShaders(
        const Handle<AssetRegistry>& engineRegistry,
        const String& target,
        const FilePath& targetConfigPath,
        ShaderTarget& outShaderTarget,
        Array<Handle<AssetObject>>& outAssets)
    {
        if (target == "web")
        {
            outShaderTarget.compileTarget = { ShaderCompileTargetPlatform::Web, ShaderCompileTargetBackend::WebGPU };
        }
        else if (target == "android")
        {
            outShaderTarget.compileTarget = { ShaderCompileTargetPlatform::Android, ShaderCompileTargetBackend::Vulkan };
        }
        else if (target == "host")
        {
            outShaderTarget.compileTarget = HostShaderCompileTarget;
        }
        else
        {
            //.. @TODO Others ...

            return HYP_MAKE_ERROR(Error, "Unknown shader target '{}', expected web, android or host", target);
        }

        outShaderTarget.isEnabled = true;
        outShaderTarget.name = target;

        JSON::Value& targetConfig = outShaderTarget.config;

        if (targetConfigPath.Any())
        {
            FileByteReader configReader { targetConfigPath };

            if (configReader.Eof())
            {
                return HYP_MAKE_ERROR(Error, "Failed to read target config '{}'", targetConfigPath);
            }

            JSON::ParseResult configParseResult = JSON::Parse(String(configReader.Read().ToByteView()));

            if (!configParseResult.ok)
            {
                return HYP_MAKE_ERROR(Error, "Failed to parse target config '{}': {}", targetConfigPath, configParseResult.message);
            }

            targetConfig = std::move(configParseResult.value);
        }

        const FilePath shadersPath = EngineGlobals::GetConfigDirectory() / "Shaders.hmf";

        FileByteReader shadersReader { shadersPath };

        HMF::ErrorList errorList;
        HMF::ParseResult parseResult = HMF::Parse(shadersPath, shadersReader, &errorList);

        if (parseResult.HasError() || !parseResult.GetValue().Is<ShaderDefinitions>())
        {
            return HYP_MAKE_ERROR(Error, "Failed to read shader definitions from '{}'", shadersPath);
        }

        const ShaderDefinitions& definitions = parseResult.GetValue().Get<ShaderDefinitions>();

        Array<String> passShaderNames;

        for (const ShaderDefinition& definition : definitions.definitions)
        {
            if (definition.cookKind == ShaderCookKind::Material)
            {
                outShaderTarget.materialShaderNames.Insert(definition.name);

                continue;
            }

            if (definition.cookKind != ShaderCookKind::Pass || !CheckCondition(targetConfig, definition.condition))
            {
                continue;
            }

            passShaderNames.PushBack(String(*definition.name));
        }

        HYP_LOG(Assets, Info, "Compiling {} pass shader(s) of {} for {}", passShaderNames.Size(), definitions.definitions.Size(), target);

        if (!TaskSystem::GetInstance().IsRunning())
        {
            TaskSystem::GetInstance().Start();
        }

        if (!g_shaderCompiler)
        {
            g_shaderCompiler = new ShaderCompiler;
        }

        ShaderCompileParams params;
        params.SetTargetPlatforms(outShaderTarget.compileTarget.platform);
        params.SetTargetBackends(outShaderTarget.compileTarget.backend);
        params.shaderFilters = passShaderNames;
        params.exactShaderFilters = true;

        // the editor cooks in its own process, and its compiler has to keep compiling everything afterwards
        const ShaderCompileParams previousParams = g_shaderCompiler->GetCompileParams();

        if (!g_shaderCompiler->Initialize(/* precompileShaders */ true, params))
        {
            HYP_LOG(Assets, Warning, "Some pass shaders failed to compile for {} (see the errors above); the package goes without them", target);
        }

        g_shaderCompiler->Initialize(/* precompileShaders */ false, previousParams);

        uint32 numVariants = 0;

        for (const String& shaderName : passShaderNames)
        {
            Handle<AssetObject> bundleAsset = engineRegistry->GetAsset(AssetBuckets::ShaderBundles, GetShaderBundleAssetName(CreateNameFromDynamicString(shaderName.Data()), outShaderTarget.compileTarget));

            if (!bundleAsset.IsValid())
            {
                HYP_LOG(Assets, Warning, "No shader bundle for '{}' after compiling it for {}", shaderName, target);
                continue;
            }

            outAssets.PushBack(bundleAsset);

            for (const Handle<Shader>& shader : static_cast<const ShaderBundle*>(bundleAsset.Get())->compiledShaders)
            {
                if (shader.IsValid() && GetShaderCompileTargetForProperties(shader->properties) == outShaderTarget.compileTarget)
                {
                    outAssets.PushBack(shader);
                    ++numVariants;
                }
            }
        }

        HYP_LOG(Assets, Info, "{} pass shader variant(s) for {}", numVariants, target);

        return {};
    }

#ifdef HYP_EDITOR
    struct ScenePassNeeds
    {
        bool hasPointShadows = false;
        // per kind of env probe: bit 0 for EPF_VISIBILITY, bit 1 for EPF_HIT_MASK
        Set<uint32> envProbeKinds;
    };

    static void CollectScenePassNeeds(const Node* node, ScenePassNeeds& outNeeds)
    {
        if (node == nullptr)
        {
            return;
        }

        if (node->IsA<PointLight>() && (static_cast<const PointLight*>(node)->GetLightFlags() & LightFlags::ShadowCaster))
        {
            outNeeds.hasPointShadows = true;
        }

        if (node->IsA<EnvProbe>())
        {
            const EnvProbe* envProbe = static_cast<const EnvProbe*>(node);

            if (!envProbe->IsSkyProbe())
            {
                const EnumFlags<EnvProbeFlags> envProbeFlags = envProbe->GetEnvProbeFlags();

                outNeeds.envProbeKinds.Insert((bool(envProbeFlags & EPF_VISIBILITY) ? 0x1u : 0x0u) | (bool(envProbeFlags & EPF_HIT_MASK) ? 0x2u : 0x0u));
            }
        }

        for (const Handle<Node>& child : node->GetChildren())
        {
            CollectScenePassNeeds(child.Get(), outNeeds);
        }
    }

    // Compiles the variants the cooked materials draw with in every pass the target runs, decided by the renderer's own
    // GeometryPass::BuildMaterialAttributes, then collects them from their bundles.
    static void CompileMaterialShaders(
        const Handle<AssetRegistry>& engineRegistry,
        const Array<Handle<AssetObject>>& assetsToCook,
        bool usesTerrain,
        const Array<MaterialAttributes>& runtimeMaterials,
        const ShaderTarget& shaderTarget,
        Array<Handle<AssetObject>>& outAssets)
    {
        struct MaterialInput
        {
            MaterialAttributes attributes;
            bool isEmissive = false;
        };

        Array<MaterialInput> materials;
        bool hasInstancing = false;
        bool hasSkinning = false;
        ScenePassNeeds scenePassNeeds;

        for (const Handle<AssetObject>& assetObject : assetsToCook)
        {
            if (!assetObject.IsValid())
            {
                continue;
            }

            const AssetBucket& bucket = assetObject->GetPath().GetBucket();

            if (bucket == AssetBuckets::Materials)
            {
                const Material* material = static_cast<const Material*>(assetObject.Get());

                materials.PushBack(MaterialInput { material->GetAttributes(), RenderHelpers::IsEmissive(material->GetParameters()) });
            }
            else if (bucket == AssetBuckets::InstanceData || bucket == AssetBuckets::Terrain)
            {
                // instance groups, and ground cover planted on terrain
                hasInstancing = true;
            }
            else if (bucket == AssetBuckets::Skeletons)
            {
                hasSkinning = true;
            }
            else if (bucket == AssetBuckets::Scenes)
            {
                CollectScenePassNeeds(static_cast<const Scene*>(assetObject.Get())->GetRoot().Get(), scenePassNeeds);
            }
        }

        const JSON::Value& targetConfig = shaderTarget.config;
        const bool lightmapVolumes = GetTargetBool(targetConfig, "Rendering.LightmapVolumes", true);
        const bool pathTracing = GetTargetBool(targetConfig, "Rendering.PathTracing", false);
        const bool forwardDDGI = GetTargetBool(targetConfig, "Rendering.DDGI", false);

        ShaderPropertyDictionary& propertyDictionary = ShaderPropertyDictionary::GetInstance();

        // the views that draw meshes with a shader of their own; the main view draws with the material's
        Array<MaterialAttributes> passOverrides;

        if (GetTargetBool(targetConfig, "Rendering.Shadows.Enabled", true))
        {
            MaterialAttributes& shadowPass = passOverrides.EmplaceBack();
            shadowPass.shaderName = NAME("DrawShadowMap");
        }

        if (scenePassNeeds.hasPointShadows)
        {
            MaterialAttributes& pointShadowPass = passOverrides.EmplaceBack();
            pointShadowPass.shaderName = NAME("DrawCubemap");
            pointShadowPass.shaderProperties.Add(propertyDictionary.Intern(ShaderProperty(NAME("MODE_SHADOWS"))));
        }

        for (const uint32 envProbeKind : scenePassNeeds.envProbeKinds)
        {
            MaterialAttributes& envProbePass = passOverrides.EmplaceBack();
            envProbePass.shaderName = NAME("DrawCubemap");
            envProbePass.shaderProperties.Add(propertyDictionary.Intern(ShaderProperty(NAME("FORWARD_SHADING"))));
            envProbePass.shaderProperties.Add(propertyDictionary.Intern(ShaderProperty(NAME("APPLY_LIGHTMAPS"))));

            if (envProbeKind & 0x1u)
            {
                envProbePass.shaderProperties.Add(propertyDictionary.Intern(ShaderProperty(NAME("WRITE_MOMENTS"))));
            }

            if (envProbeKind & 0x2u)
            {
                envProbePass.shaderProperties.Add(propertyDictionary.Intern(ShaderProperty(NAME("WRITE_HIT_MASK"))));
            }
        }

        struct VariantRequest
        {
            Name shaderName;
            ShaderPropertySet properties;
        };

        Array<VariantRequest> requests;
        Set<HashCode> seenRequests;

        const auto addRequest = [&](const MaterialAttributes& materialAttributes, const ShaderPropertySet& properties)
        {
            if (!shaderTarget.materialShaderNames.Contains(materialAttributes.shaderName))
            {
                return;
            }

            HashCode hashCode = materialAttributes.shaderName.GetHashCode();
            hashCode.Add(properties.GetHashCode());

            if (seenRequests.Insert(hashCode).second)
            {
                requests.PushBack(VariantRequest { materialAttributes.shaderName, properties });
            }
        };

        if (usesTerrain)
        {
            materials.PushBack(MaterialInput { TerrainWorldGridLayer::GetTerrainMaterialAttributes(), false });
        }

        for (const MaterialAttributes& runtimeMaterial : runtimeMaterials)
        {
            materials.PushBack(MaterialInput { runtimeMaterial, false });
        }

        for (const MaterialInput& material : materials)
        {
            GeometryPass::DrawMaterialInputs inputs;
            inputs.materialAttributes = material.attributes;
            inputs.isEmissive = material.isEmissive;
            inputs.lightmapVolumes = lightmapVolumes;
            inputs.pathTracing = pathTracing;

            const bool canBeLightmapped = lightmapVolumes && inputs.materialAttributes.bucket == RenderBucket::Lightmapped;

            for (uint32 combination = 0; combination < 8; combination++)
            {
                inputs.hasInstancing = (combination & 0x1u) != 0;
                inputs.hasSkinning = (combination & 0x2u) != 0;
                inputs.lightmapStencilValue = (combination & 0x4u) != 0 ? 1 : 0;

                if ((inputs.hasInstancing && !hasInstancing) || (inputs.hasSkinning && !hasSkinning) || (inputs.lightmapStencilValue != 0 && !canBeLightmapped))
                {
                    continue;
                }

                MaterialAttributes materialAttributes;
                GeometryPass::BuildMaterialAttributes(inputs, nullptr, materialAttributes);
                addRequest(materialAttributes, GeometryPass::GetDrawShaderProperties(materialAttributes, /* clusteredForwardLighting */ true, forwardDDGI));

                for (const MaterialAttributes& passOverride : passOverrides)
                {
                    GeometryPass::BuildMaterialAttributes(inputs, &passOverride, materialAttributes);
                    addRequest(materialAttributes, materialAttributes.shaderProperties);
                }
            }
        }

        // the renderer adds bindless textures where the device supports them, so a target that can have them gets both variants
        if (shaderTarget.compileTarget.backend != ShaderCompileTargetBackend::WebGPU)
        {
            const ShaderPropertyId bindlessTexturesPropertyId = propertyDictionary.Intern(ShaderProperty(NAME("HYP_FEATURES_BINDLESS_TEXTURES")));
            const size_t numRequests = requests.Size();

            for (size_t requestIndex = 0; requestIndex < numRequests; requestIndex++)
            {
                VariantRequest bindlessRequest = requests[requestIndex];
                bindlessRequest.properties.Add(bindlessTexturesPropertyId);

                requests.PushBack(std::move(bindlessRequest));
            }
        }

        Map<Name, uint32> numRequestsPerShader;

        for (const VariantRequest& request : requests)
        {
            ++numRequestsPerShader[request.shaderName];
        }

        HYP_LOG(Assets, Info, "Compiling {} material shader variant(s) for the {} cooked material(s) and {} pass override(s){}{}: {}",
            requests.Size(), materials.Size(), passOverrides.Size(),
            scenePassNeeds.hasPointShadows ? ", point light shadows" : "",
            scenePassNeeds.envProbeKinds.Any() ? ", env probes" : "",
            String::Join(numRequestsPerShader, ", ", [](const auto& it)
                {
                    return HYP_FORMAT("{} {}", it.second, it.first);
                }));

        const ShaderPropertyId numGBufferTexturesPropertyId = propertyDictionary.Intern(ShaderProperty(NAME("NUM_GBUFFER_TEXTURES"), int(NumGBufferTargets)));

        Map<Name, Map<HashCode, ShaderPropertySet>> requestedVariants;

        for (const VariantRequest& request : requests)
        {
            if (!g_shaderCompiler->CompileVariantForTarget(request.shaderName, request.properties, VertexInputLayoutDesc { VT_Simple }, shaderTarget.compileTarget))
            {
                HYP_LOG(Assets, Warning, "Failed to compile a {} variant for {} (see the errors above)", request.shaderName, shaderTarget.name);
                continue;
            }

            ShaderPropertySet targetProperties = request.properties;
            AddShaderCompileTargetProperties(shaderTarget.compileTarget, targetProperties);

            requestedVariants[request.shaderName][targetProperties.GetHashCode()] = targetProperties;
        }

        uint32 numVariants = 0;

        for (const auto& it : requestedVariants)
        {
            Handle<AssetObject> bundleAsset = engineRegistry->GetAsset(AssetBuckets::ShaderBundles, GetShaderBundleAssetName(it.first, shaderTarget.compileTarget));

            if (!bundleAsset.IsValid())
            {
                continue;
            }

            const ShaderBundle* bundle = static_cast<const ShaderBundle*>(bundleAsset.Get());

            Set<HashCode> collectedVariants;

            outAssets.PushBack(bundleAsset);

            for (const Handle<Shader>& shader : bundle->compiledShaders)
            {
                if (!shader.IsValid())
                {
                    continue;
                }

                // a variant carries its bundle's STATIC() properties and the global ones on top of what was requested
                ShaderPropertySet requestedProperties = shader->properties;
                requestedProperties.Set(numGBufferTexturesPropertyId, false);

                for (const ShaderPropertyId staticPropertyId : bundle->staticProperties)
                {
                    requestedProperties.Set(staticPropertyId, false);
                }

                if (it.second.Contains(requestedProperties.GetHashCode()))
                {
                    outAssets.PushBack(shader);
                    ++numVariants;
                    collectedVariants.Insert(requestedProperties.GetHashCode());
                }
            }

            for (const auto& requested : it.second)
            {
                if (!collectedVariants.Contains(requested.first))
                {
                    HYP_LOG(Assets, Warning, "Compiled {} {} for {} but found no matching variant in its bundle",
                        it.first, requested.second.GetDebugString(), shaderTarget.name);
                }
            }
        }

        HYP_LOG(Assets, Info, "{} material shader variant(s) for {}", numVariants, shaderTarget.name);
    }
#endif // HYP_EDITOR

    static Result CookListedAssets(
        AssetRegistry* registry,
        const FilePath& assetListPath,
        const Array<Handle<AssetObject>>& targetShaderAssets,
        const FilePath& outputContentDir,
        Array<TSharedResLock<AssetObject>>& readLocks,
        Array<CollectedBlob>& collectedBlobs,
        Array<uint64>& blockSizes,
        Set<const AssetObject*>& cookedAssets,
        const Set<uint8>& projectMeshLayouts)
    {
        FILE* listFile = nullptr;

        if (assetListPath.Any())
        {
            listFile = fopen(assetListPath.Data(), "r");

            if (listFile == nullptr)
            {
                return HYP_MAKE_ERROR(Error, "Failed to open asset list '{}'", assetListPath);
            }
        }

        HYP_DEFER({
            if (listFile != nullptr)
            {
                fclose(listFile);
            }
        });

        Set<String> seenEntries;
        Array<Handle<AssetObject>> listedAssets;
        Set<const AssetObject*> listedAssetSet;
        uint32 numMissing = 0;

        for (const Handle<AssetObject>& assetObject : targetShaderAssets)
        {
            if (assetObject.IsValid() && listedAssetSet.Insert(assetObject.Get()).second)
            {
                listedAssets.PushBack(assetObject);
            }
        }

        char line[1024];

        while (listFile != nullptr && fgets(line, sizeof(line), listFile) != nullptr)
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

            if (listedAssetSet.Insert(assetObject.Get()).second)
            {
                listedAssets.PushBack(std::move(assetObject));
            }
        }

        /// meshes the engine builds at run time (debug draw, particles, MeshBuilder shapes) are VT_Simple.
        /// terrain adds its own layout
        Set<uint8> meshLayouts = projectMeshLayouts;
        meshLayouts.Insert(uint8(VT_Simple));

        for (const Handle<AssetObject>& assetObject : listedAssets)
        {
            if (assetObject->GetPath().GetBucket() == AssetBuckets::Meshes)
            {
                meshLayouts.Insert(static_cast<const Mesh*>(assetObject.Get())->GetMeshAttributes().inputLayout.mask);
            }
        }

        Set<const AssetObject*> unusedLayoutShaders;

        for (const Handle<AssetObject>& assetObject : listedAssets)
        {
            if (assetObject->GetPath().GetBucket() != AssetBuckets::ShaderBundles)
            {
                continue;
            }

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

            // a request compiles every layout the bundle's optional attributes allow, but only the cooked meshes' layouts are drawn
            const bool drawsMeshLayouts = AnyOf(shaderBundle->compiledShaders, [&meshLayouts](const Handle<Shader>& shader)
                {
                    return meshLayouts.Contains(shader->inputLayout.mask);
                });

            if (!drawsMeshLayouts)
            {
                continue;
            }

            for (auto it = shaderBundle->compiledShaders.Begin(); it != shaderBundle->compiledShaders.End();)
            {
                if (!meshLayouts.Contains((*it)->inputLayout.mask))
                {
                    unusedLayoutShaders.Insert(static_cast<const AssetObject*>(it->Get()));

                    it = shaderBundle->compiledShaders.Erase(it);

                    continue;
                }

                ++it;
            }
        }

        for (const Handle<AssetObject>& assetObject : listedAssets)
        {
            if (unusedLayoutShaders.Contains(assetObject.Get()))
            {
                continue;
            }

            if (Result result = CookAsset(outputContentDir, assetObject, readLocks, collectedBlobs, blockSizes, cookedAssets); result.HasError())
            {
                return result;
            }
        }

        HYP_LOG(Assets, Info, "Cooked {} listed asset(s) from \"{}\" and the {} shader asset(s) compiled for the target, {} missing, {} shader variant(s) left out for vertex layouts no cooked mesh has",
            listedAssets.Size() - unusedLayoutShaders.Size(), assetListPath, targetShaderAssets.Size(), numMissing, unusedLayoutShaders.Size());

        return {};
    }

    static Result Cook(
        const Handle<AssetRegistry>& engineRegistry, const Handle<AssetRegistry>& gameRegistry,
        const FilePath& projectPath,
        const Array<String>& worldNames,
        const FilePath& engineAssetListPath,
        const ShaderTarget& shaderTarget,
        Array<Handle<AssetObject>> targetShaderAssets,
        const FilePath& outputCacheDir, const FilePath& outputContentDir, const FilePath& outputEngineContentDir)
    {
        Array<TSharedResLock<AssetObject>> readLocks;
        Array<CollectedBlob> collectedBlobs;

        Array<uint64> blockSizes;
        blockSizes.Resize(MaxAssetBuckets);

        Set<const AssetObject*> cookedAssets;

        // the project's walk reaches engine assets, so the engine registry has to be ready before it
        if (engineRegistry)
        {
            SetEngineAssetRegistry(engineRegistry);

            GlobalContextScope assetRegistryContextScope { AssetRegistryContext { engineRegistry } };
            engineRegistry->LoadAssetDescs();
        }

        Array<Handle<AssetObject>> assetsToCook;
        Set<uint8> projectMeshLayouts;

#ifdef HYP_EDITOR
        // Game content: only cook what's actually reachable from the project's own data to strip assets that are unused.
        if (gameRegistry)
        {
            GlobalContextScope assetRegistryContextScope { AssetRegistryContext { gameRegistry } };
            gameRegistry->LoadAssetDescs();

            bool usesTerrain = false;
            Array<MaterialAttributes> runtimeMaterials;

            TResult<Handle<EditorProject>> loadProjectResult = EditorProject::Load(projectPath);

            if (loadProjectResult.HasValue())
            {
                const Handle<PackagingSettings>& packagingSettings = loadProjectResult.GetValue()->GetPackagingSettings();

                if (s_hasTargetPlatform && !s_hasMaxTextureSizeArgument && packagingSettings.IsValid())
                {
                    s_maxTextureSize = packagingSettings->Resolve(s_targetPlatform).content.maxTextureSize;
                }

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

                static const Name s_terrainLayerClassName = NAME("TerrainWorldGridLayer");

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

                    const Handle<World>& world = TryCast<World>(worldAsset);

                    if (world.IsValid() && world->GetWorldGrid().IsValid())
                    {
                        for (const Handle<WorldGridLayer>& layer : world->GetWorldGrid()->GetLayers())
                        {
                            usesTerrain |= layer.IsValid() && layer->InstanceClass()->GetName() == s_terrainLayerClassName;
                        }
                    }
                }

                HYP_LOG(Assets, Info, "Found {} asset(s) reachable from project at \"{}\"", assetsToCook.Size(), projectPath);

                AddEngineDefaults(engineRegistry, usesTerrain, callback);
                AddPluginContent(engineRegistry, callback);

                const EditorCookSettings& cookSettings = loadProjectResult.GetValue()->GetPlatformSettings().cook;

                runtimeMaterials = cookSettings.runtimeMaterials;

                for (const AssetPath& additionalAsset : cookSettings.additionalAssets)
                {
                    Handle<AssetObject> asset = AssetReference(additionalAsset).Resolve();

                    if (!asset.IsValid())
                    {
                        HYP_LOG(Assets, Warning, "Additional asset {} is not in the project", additionalAsset);
                        continue;
                    }

                    AssetRegistry::WalkAssetDeep(BoxedValue(asset), callback);
                }

                HYP_LOG(Assets, Info, "{} asset(s) to cook with the engine defaults{} and the additional assets", assetsToCook.Size(), usesTerrain ? " (terrain included)" : "");
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

                // the engine code still needs its own assets, and with no worlds walked, terrain can't be ruled out
                usesTerrain = true;

                Set<AssetObject*> seenAssets;

                for (const Handle<AssetObject>& assetObject : assetsToCook)
                {
                    seenAssets.Insert(assetObject.Get());
                }

                auto addUnseen = [&assetsToCook, &seenAssets](const Handle<AssetObject>& assetObject)
                    {
                        if (assetObject.IsValid() && seenAssets.Insert(assetObject.Get()).second)
                        {
                            assetsToCook.PushBack(assetObject);
                        }
                    };

                AddEngineDefaults(engineRegistry, usesTerrain, addUnseen);
                AddPluginContent(engineRegistry, addUnseen);
            }

            for (const Handle<AssetObject>& assetObject : assetsToCook)
            {
                if (assetObject.IsValid() && assetObject->GetPath().GetBucket() == AssetBuckets::Meshes)
                {
                    projectMeshLayouts.Insert(static_cast<const Mesh*>(assetObject.Get())->GetMeshAttributes().inputLayout.mask);
                }
            }

            // terrain cells are built at run time with a second UV set
            if (usesTerrain)
            {
                projectMeshLayouts.Insert(uint8(VT_Simple | VT_UV1));
            }

            if (shaderTarget.isEnabled && engineRegistry)
            {
                CompileMaterialShaders(engineRegistry, assetsToCook, usesTerrain, runtimeMaterials, shaderTarget, targetShaderAssets);
            }
        }
#endif // HYP_EDITOR

        if (engineRegistry)
        {
            GlobalContextScope assetRegistryContextScope { AssetRegistryContext { engineRegistry } };

            if (engineAssetListPath.Any() || targetShaderAssets.Any())
            {
                if (Result result = CookListedAssets(engineRegistry, engineAssetListPath, targetShaderAssets, outputEngineContentDir, readLocks, collectedBlobs, blockSizes, cookedAssets, projectMeshLayouts); result.HasError())
                {
                    return result;
                }
            }
            else
            {
                for (uint32 bucketIndex = 1; bucketIndex < MaxAssetBuckets; bucketIndex++)
                {
                    if (Result result = CookBucketInFull(engineRegistry, bucketIndex, outputEngineContentDir, readLocks, collectedBlobs, blockSizes, cookedAssets); result.HasError())
                    {
                        return result;
                    }
                }
            }
        }

#ifdef HYP_EDITOR
        if (gameRegistry)
        {
            GlobalContextScope assetRegistryContextScope { AssetRegistryContext { gameRegistry } };

            uint32 numEngineAssets = 0;

            FindDataOnlyTextures(assetsToCook);

            for (const Handle<AssetObject>& assetObject : assetsToCook)
            {
                if (!assetObject.IsValid())
                {
                    continue;
                }

                const FilePath* assetOutputContentDir = &outputContentDir;

                switch (assetObject->GetPath().registryId)
                {
                case AssetRegistryId::Game:
                    break;
                case AssetRegistryId::Engine:
                    assetOutputContentDir = &outputEngineContentDir;
                    ++numEngineAssets;
                    break;
                default:
                    HYP_LOG(Assets, Warning, "Skipping \"{}\", reached from the project but owned by the {} registry",
                        assetObject->GetPath(), GetAssetRegistryName(assetObject->GetPath().registryId));
                    continue;
                }

                if (Result result = CookAsset(*assetOutputContentDir, assetObject, readLocks, collectedBlobs, blockSizes, cookedAssets); result.HasError())
                {
                    return result;
                }
            }

            HYP_LOG(Assets, Info, "{} of the reachable asset(s) are engine content, cooked to \"{}\"", numEngineAssets, outputEngineContentDir);
        }
#endif // HYP_EDITOR

        DeduplicateBlobs(collectedBlobs, blockSizes);

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

            if (collectedBlob.aliasOf != InvalidBlobIndex)
            {
                if (!cookedStorage.PutAlias(collectedBlob.key, collectedBlobs[collectedBlob.aliasOf].key))
                {
                    return HYP_MAKE_ERROR(Error, "Failed to point cooked blob key {} at its duplicate", collectedBlob.key.GetHashCode().Value());
                }

                continue;
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
        g_shaderManager->WriteShaderCache(outputCacheDir, /* writePreloadEntries */ !shaderTarget.isEnabled);
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
