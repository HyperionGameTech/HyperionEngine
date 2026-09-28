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

struct GroundCoverSource
{
    const char* name;
    float weight;
    uint32 splatLayer;
};

static constexpr GroundCoverSource s_groundCoverSources[] = {
    { "meadow_grass", 6.0f, 0 },
    { "short_grass", 3.0f, 0 },
    { "wildflower_meadow", 1.5f, 0 },
    { "dry_grass", 1.0f, 2 }
};

static void BuildInvSphere(Handle<AssetRegistry>& engineRegistry)
{
    GlobalContextScope assetRegistryScope { AssetRegistryContext { engineRegistry } };

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
    
    engineRegistry->RemoveAsset(mc.mesh);
    engineRegistry->RemoveAsset(mc.material);

    mc.mesh->SetName(NAME("InvSphereMesh"));
    mc.material->SetName(NAME("InvSphereMaterial"));

    engineRegistry->PutAssetsDeep(prefab, /* overwriteExisting */ true);

    HYP_LOG(Engine, Info, "InvSphere shape built and registered successfully.");
}

static constexpr float ThirdPersonCharacterHeight = 1.8f;

static constexpr const char* DefaultThirdPersonCharacterSource = "Models/Mannequin/Mannequin.glb";

static void BuildThirdPersonCharacter(Handle<AssetRegistry>& engineRegistry, const String& sourcePath)
{
    GlobalContextScope assetRegistryScope { AssetRegistryContext { engineRegistry } };

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

    engineRegistry->PutAssetsDeep(prefab, /* overwriteExisting */ true);

    HYP_LOG(Engine, Info, "ThirdPersonCharacter prefab built and registered successfully (model height {}, scale {}).", modelHeight, root->GetLocalScale().x);
}

static void BuildGroundCover(Handle<AssetRegistry>& engineRegistry)
{
    GlobalContextScope assetRegistryScope { AssetRegistryContext { engineRegistry } };

    Handle<GroundCover> groundCover = MakeHandle<GroundCover>(NAME("DefaultGroundCover"));

    for (const GroundCoverSource& source : s_groundCoverSources)
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

            for (const Handle<Texture>& texture : meshComponent->material->GetTextures())
            {
                prefixName(texture);
            }
        }

        engineRegistry->PutAssetsDeep(prefab, /* overwriteExisting */ true);

        GroundCoverLayer* layer = nullptr;

        for (GroundCoverLayer& existingLayer : groundCover->layers)
        {
            if (existingLayer.splatLayer == source.splatLayer)
            {
                layer = &existingLayer;
            }
        }

        if (layer == nullptr)
        {
            layer = &groundCover->layers.EmplaceBack();
            layer->splatLayer = source.splatLayer;
        }

        layer->types.PushBack(GroundCoverType { prefab, source.weight });

        HYP_LOG(Engine, Info, "Ground cover {} built and registered", source.name);
    }

    InitObject(groundCover);

    engineRegistry->PutAssetsDeep(groundCover, /* overwriteExisting */ true);
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

        if (IsOnThread(g_simThread))
        {
            RunStatic(characterSource, parts);
        }
        else
        {
            GetThreadById(g_simThread)->GetScheduler().Enqueue(
                [characterSource, parts]()
                {
                    RunStatic(characterSource, parts);
                },
                TaskEnqueueFlags::FIRE_AND_FORGET);
        }

        return {};
    }

    static void RunStatic(const String& characterSource, const String& parts)
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

        if (isSelected("invsphere"))
        {
            BuildInvSphere(engineRegistry);
        }

        if (isSelected("character"))
        {
            BuildThirdPersonCharacter(engineRegistry, characterSource);
        }

        if (isSelected("groundcover"))
        {
            BuildGroundCover(engineRegistry);
        }

        GlobalContextScope assetRegistryScope { AssetRegistryContext { engineRegistry } };
        GetCurrentAssetRegistry()->SaveDirtyAssets();

        HYP_LOG(Engine, Info, "Shape assets saved to engine registry");
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
