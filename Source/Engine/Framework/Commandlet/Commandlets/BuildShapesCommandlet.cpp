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

#include <Rendering/Mesh.hpp>
#include <Rendering/Material.hpp>

#include <Framework/EngineGlobals.hpp>

namespace Hyperion {

#ifdef HYP_EDITOR

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

static void BuildThirdPersonCharacter(Handle<AssetRegistry>& engineRegistry)
{
    GlobalContextScope assetRegistryScope { AssetRegistryContext { engineRegistry } };

    auto characterPrefabResult = g_assetManager->Load<Prefab>("Models/Hazmat/Hazmat.glb",
        String::empty,
        AssetLoadHint::Transient);

    if (!characterPrefabResult.HasValue())
    {
        HYP_LOG(Engine, Error, "Failed to load source Models/Hazmat/Hazmat.glb to build the ThirdPersonCharacter prefab");

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


class BuildShapesCommandlet : public CommandletBase
{
    HYP_OBJECT_BODY(BuildShapesCommandlet);

public:
    virtual ~BuildShapesCommandlet() override = default;

protected:
    virtual Result Run(const CommandLineArguments& args) override
    {
        if (IsOnThread(g_simThread))
        {
            RunStatic();
        }
        else
        {
            GetThreadById(g_simThread)->GetScheduler().Enqueue(RunStatic, TaskEnqueueFlags::FIRE_AND_FORGET);
        }

        return {};
    }

    static void RunStatic()
    {
        Handle<AssetRegistry> engineRegistry = GetEngineAssetRegistry();

        if (!engineRegistry.IsValid())
        {
            engineRegistry = MakeHandle<AssetRegistry>(
                AssetRegistryId::Engine,
                EngineGlobals::GetContentDirectory<HYP_STATIC_STRING("Engine")>());

            engineRegistry->Initialize(nullptr);

            SetEngineAssetRegistry(engineRegistry);
        }

        BuildInvSphere(engineRegistry);
        BuildThirdPersonCharacter(engineRegistry);

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
HYP_END_CLASS

HYP_REGISTER_STATIC_CLASS(BuildShapesCommandlet);

#endif // HYP_EDITOR

} // namespace Hyperion
