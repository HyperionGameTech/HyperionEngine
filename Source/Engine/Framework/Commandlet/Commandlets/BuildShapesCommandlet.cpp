#include <HyperionPch.hpp>

#include <Framework/Commandlet/Commandlet.hpp>

#include <Core/Reflection/ClassUtils.hpp>
#include <Core/Reflection/ClassRegistry.hpp>

#include <Core/CLI/CommandLine.hpp>

#include <Asset/Assets.hpp>
#include <Asset/AssetRegistry.hpp>

#include <Scene/Node.hpp>
#include <Scene/Entity.hpp>
#include <Scene/Prefab.hpp>
#include <Scene/EntityManager.hpp>

#include <Scene/Components/MeshComponent.hpp>

#include <Scene/WorldGrid/Terrain/GroundCover.hpp>

#include <Rendering/Mesh.hpp>
#include <Rendering/Material.hpp>
#include <Rendering/Texture.hpp>

#include <Framework/EngineGlobals.hpp>

namespace Hyperion {

#ifdef HYP_EDITOR

struct GroundCoverPrefabSource
{
    const char* name;
    const char* layerName;
    GroundCoverSource source;
    uint32 splatLayer;
};

static constexpr GroundCoverPrefabSource s_groundCoverSources[] = {
    { "meadow_grass", "Meadow", GroundCoverSource::SplatLayer, 0 },
    { "short_grass", "Short Grass", GroundCoverSource::Painted, 0 },
    { "wildflower_meadow", "Wildflowers", GroundCoverSource::Painted, 0 },
    { "dry_grass", "Dry Grass", GroundCoverSource::Painted, 0 }
};

static void BuildInvSphere(Handle<AssetRegistry>& outputRegistry)
{
    GlobalContextScope assetRegistryScope { AssetRegistryContext { outputRegistry } };

    auto domePrefabResult = g_assetManager->Load<Prefab>("Models/inv_sphere.obj",
        String::empty,
        AssetLoadHint::Transient);

    if (!domePrefabResult.HasValue())
    {
        HYP_LOG(Engine, Error, "Failed to load source inv_sphere.obj to build InvSphere shape");

        return;
    }

    Handle<Prefab> prefab = domePrefabResult->Result();
    Assert(prefab.IsValid());

    prefab->SetName(NAME("InvSphere"));
    
    Handle<Entity> e = StaticCast<Entity>(prefab->GetRoot()->GetChild(0));

    MeshComponent& mc = e->GetComponent<MeshComponent>();
    
    outputRegistry->RemoveAsset(mc.mesh);
    outputRegistry->RemoveAsset(mc.material);

    mc.mesh->SetName(NAME("InvSphereMesh"));
    mc.material->SetName(NAME("InvSphereMaterial"));

    outputRegistry->PutAssetsDeep(prefab, /* overwriteExisting */ true);

    HYP_LOG(Engine, Info, "InvSphere shape built and registered successfully.");
}

static constexpr float ThirdPersonCharacterHeight = 1.8f;

static constexpr const char* DefaultThirdPersonCharacterSource = "Models/Mannequin/Mannequin.glb";

static void BuildThirdPersonCharacter(Handle<AssetRegistry>& outputRegistry, const String& sourcePath)
{
    GlobalContextScope assetRegistryScope { AssetRegistryContext { outputRegistry } };

    auto characterPrefabResult = g_assetManager->Load<Prefab>(sourcePath,
        String::empty,
        AssetLoadHint::Transient);

    if (!characterPrefabResult.HasValue())
    {
        HYP_LOG(Engine, Error, "Failed to load source {} to build the ThirdPersonCharacter prefab", sourcePath);

        return;
    }

    Handle<Prefab> prefab = characterPrefabResult->Result();
    Assert(prefab.IsValid());

    prefab->SetName(NAME("ThirdPersonCharacter"));

    const Handle<Node>& root = prefab->GetRoot();

    BoundingBox modelBounds;

    for (Node* descendant : root->GetDescendants())
    {
        if (!descendant->IsA<Entity>())
        {
            continue;
        }

        if (const MeshComponent* meshComponent = static_cast<Entity*>(descendant)->TryGetComponent<MeshComponent>(); meshComponent && meshComponent->mesh)
        {
            modelBounds = modelBounds.Union(meshComponent->mesh->GetAABB());
        }
    }

    const float modelHeight = modelBounds.GetExtent().y;

    if (modelHeight > 0.0f)
    {
        root->SetLocalScale(Vec3f(ThirdPersonCharacterHeight / modelHeight));
    }

    outputRegistry->PutAssetsDeep(prefab, /* overwriteExisting */ true);

    HYP_LOG(Engine, Info, "ThirdPersonCharacter prefab built and registered successfully (model height {}, scale {}).", modelHeight, root->GetLocalScale().x);
}

static void BuildGroundCover(Handle<AssetRegistry>& outputRegistry)
{
    GlobalContextScope assetRegistryScope { AssetRegistryContext { outputRegistry } };

    Handle<GroundCover> groundCover = MakeHandle<GroundCover>(NAME("DefaultGroundCover"));

    for (const GroundCoverPrefabSource& source : s_groundCoverSources)
    {
        auto prefabResult = g_assetManager->Load<Prefab>(HYP_FORMAT("Models/GroundCover/{}.glb", source.name),
            String::empty,
            AssetLoadHint::Transient);

        if (!prefabResult.HasValue())
        {
            HYP_LOG(Engine, Error, "Failed to load ground cover source {}.glb", source.name);

            continue;
        }

        Handle<Prefab> prefab = prefabResult->Result();
        Assert(prefab.IsValid());

        prefab->SetName(CreateNameFromDynamicString(source.name));

        const auto prefixName = [&source](AssetObject* asset)
        {
            const ANSIString prefix = HYP_FORMAT("{}_", source.name);

            if (asset == nullptr)
            {
                return;
            }

            const ANSIString name = asset->GetName().LookupString();

            if (!name.StartsWith(prefix))
            {
                asset->SetName(CreateNameFromDynamicString(prefix + name));
            }
        };

        for (Node* descendant : prefab->GetRoot()->GetDescendants())
        {
            if (!descendant->IsA<Entity>())
            {
                continue;
            }

            const MeshComponent* meshComponent = static_cast<Entity*>(descendant)->TryGetComponent<MeshComponent>();

            if (!meshComponent || !meshComponent->mesh || !meshComponent->material)
            {
                continue;
            }

            prefixName(meshComponent->mesh);
            prefixName(meshComponent->material);

            MaterialParameters parameters = meshComponent->material->GetParameters();
            parameters.colorVariation = 0.6f;
            parameters.groundNormalBlend = 0.7f;
            parameters.baseOcclusion = 0.6f;
            parameters.baseOcclusionHeight = MathUtil::Max(parameters.baseOcclusionHeight, meshComponent->mesh->GetAABB().max.y * 0.5f);
            meshComponent->material->SetParameters(parameters);

            for (const Handle<Texture>& texture : meshComponent->material->GetTextures())
            {
                prefixName(texture);
            }
        }

        outputRegistry->PutAssetsDeep(prefab, /* overwriteExisting */ true);

        GroundCoverLayer& layer = groundCover->layers.EmplaceBack();
        layer.name = CreateNameFromDynamicString(source.layerName);
        layer.source = source.source;
        layer.splatLayer = source.splatLayer;
        layer.types.PushBack(GroundCoverType { prefab, 1.0f });

        HYP_LOG(Engine, Info, "Ground cover {} built and registered", source.name);
    }

    InitObject(groundCover);

    outputRegistry->PutAssetsDeep(groundCover, /* overwriteExisting */ true);
}

class BuildShapesCommandlet : public CommandletBase
{
    HYP_OBJECT_BODY(BuildShapesCommandlet);

public:
    virtual ~BuildShapesCommandlet() override = default;

    static const CommandLineArgumentDefinitions& GetArgumentDefinitions()
    {
        static CommandLineArgumentDefinitions s_definitions;
        static bool s_initialized = false;

        if (!s_initialized)
        {
            s_initialized = true;

            s_definitions.Add(
                "character",
                "c",
                "Model the ThirdPersonCharacter prefab is built from, relative to the data directory",
                CommandLineArgumentFlags::NONE,
                CommandLineArgumentType::STRING,
                JSON::Value(DefaultThirdPersonCharacterSource));

            s_definitions.Add(
                "parts",
                "p",
                "Comma separated parts to build: invsphere, character, groundcover. Everything when empty",
                CommandLineArgumentFlags::NONE,
                CommandLineArgumentType::STRING,
                JSON::Value(""));

            s_definitions.Add(
                "output",
                "o",
                "Directory of the asset registry the shapes are written to. The engine asset registry when empty",
                CommandLineArgumentFlags::NONE,
                CommandLineArgumentType::STRING,
                JSON::Value(""));
        }

        return s_definitions;
    }

protected:
    virtual Result Run(const CommandLineArguments& args) override
    {
        String characterSource = args["character"].ToString();

        if (characterSource.Empty())
        {
            characterSource = DefaultThirdPersonCharacterSource;
        }

        const String parts = args["parts"].ToString();
        const String outputPath = args["output"].ToString();

        if (IsOnThread(g_simThread))
        {
            RunStatic(characterSource, parts, outputPath);
        }
        else
        {
            GetThreadById(g_simThread)->GetScheduler().Enqueue(
                [characterSource, parts, outputPath]()
                {
                    RunStatic(characterSource, parts, outputPath);
                },
                TaskEnqueueFlags::FIRE_AND_FORGET);
        }

        return {};
    }

    static void RunStatic(const String& characterSource, const String& parts, const String& outputPath)
    {
        const Array<String> selectedParts = parts.Split(',');

        const auto isSelected = [&parts, &selectedParts](const char* part)
        {
            return parts.Empty() || selectedParts.Contains(String(part));
        };

        Handle<AssetRegistry> engineRegistry = GetEngineAssetRegistry();

        if (!engineRegistry.IsValid())
        {
            engineRegistry = MakeHandle<AssetRegistry>(
                AssetRegistryId::Engine,
                EngineGlobals::GetContentDirectory<HYP_STATIC_STRING("Engine")>());

            engineRegistry->Initialize(nullptr);

            SetEngineAssetRegistry(engineRegistry);
        }

        Handle<AssetRegistry> outputRegistry = engineRegistry;

        if (!outputPath.Empty())
        {
            // Keeps the Engine id so the built assets resolve once the output is merged into the engine content
            outputRegistry = MakeHandle<AssetRegistry>(AssetRegistryId::Engine, FilePath(outputPath));
            outputRegistry->Initialize(nullptr);
        }

        if (isSelected("invsphere"))
        {
            BuildInvSphere(outputRegistry);
        }

        if (isSelected("character"))
        {
            BuildThirdPersonCharacter(outputRegistry, characterSource);
        }

        if (isSelected("groundcover"))
        {
            BuildGroundCover(outputRegistry);
        }

        GlobalContextScope assetRegistryScope { AssetRegistryContext { outputRegistry } };
        GetCurrentAssetRegistry()->SaveDirtyAssets();

        HYP_LOG(Engine, Info, "Shape assets saved to registry at {}", outputRegistry->GetRootPath());
    }
};

HYP_EXPORT const Class* g_clsBuildShapesCommandlet = nullptr;

const Class* BuildShapesCommandlet::StaticClass()
{
    return g_clsBuildShapesCommandlet;
}

HYP_BEGIN_CLASS(BuildShapesCommandlet, -1, 0, NAME("CommandletBase"), ClassAttribute("command", "buildshapes"))
    Method(NAME("GetArgumentDefinitions"), &Type::GetArgumentDefinitions)
HYP_END_CLASS

HYP_REGISTER_STATIC_CLASS(BuildShapesCommandlet);

#endif // HYP_EDITOR

} // namespace Hyperion
