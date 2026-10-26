/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#include <EditorPch.hpp>

#include <Editor/EditorAssetDrop.hpp>
#include <Editor/EditorAction.hpp>
#include <Editor/EditorSubsystem.hpp>


#include <Scene/EntityManager.hpp>
#include <Scene/Entity.hpp>

#include <Scene/Components/MeshComponent.hpp>
#include <Scene/Components/ScriptComponent.hpp>

#include <Scripting/Asset/ScriptAsset.hpp>

#include <Rendering/Material.hpp>
#include <Rendering/Texture.hpp>

#include <Asset/AssetRegistry.hpp>

#include <System/MessageBox.hpp>

#include <Core/Utilities/Format.hpp>

#include <Core/Logging/Logger.hpp>

namespace Hyperion {

EDITOR_API HYP_DECLARE_LOG_CHANNEL(Editor);

struct EntityAssetDropHandler
{
    bool (*acceptsAsset)(const AssetObject* asset);
    
    bool (*canApply)(
        const AssetObject* asset,
        const Entity* entity);

    Handle<EditorActionBase> (*createAction)(
        EditorSubsystem* subsystem,
        const Handle<AssetObject>& asset,
        const Handle<Entity>& entity);
};

Handle<EditorActionBase> MakeEntityDropAction(
    EditorSubsystem* subsystem,
    const String& text,
    const Handle<Entity>& entity,
    Proc<void(EditorSubsystem*)>&& apply,
    Proc<void(EditorSubsystem*)>&& revert)
{
    Array<Handle<Node>> previousSelectedNodes = subsystem->GetSelectedNodes();
    WeakHandle<Node> previousFocusedNode = subsystem->GetFocusedNode();

    Handle<FunctionalEditorAction> action = MakeHandle<FunctionalEditorAction>(
        text,
        Proc<EditorActionFunctions()>(
            [entity, apply = std::move(apply), revert = std::move(revert), previousSelectedNodes, previousFocusedNode]() mutable -> EditorActionFunctions
            {
                return EditorActionFunctions {
                    .execute = Proc<void(EditorSubsystem*, EditorProject*)>(
                        [entity, apply = std::move(apply)](EditorSubsystem* editorSubsystem, EditorProject*)
                        {
                            apply(editorSubsystem);

                            editorSubsystem->SetSelectedNodes({ entity });
                            editorSubsystem->SetFocusedNode(entity, true);
                        }),
                    .revert = Proc<void(EditorSubsystem*, EditorProject*)>(
                        [revert = std::move(revert), previousSelectedNodes, previousFocusedNode](EditorSubsystem* editorSubsystem, EditorProject*)
                        {
                            revert(editorSubsystem);

                            editorSubsystem->SetSelectedNodes(previousSelectedNodes);

                            if (Handle<Node> focusedNode = previousFocusedNode.Lock(); focusedNode.IsValid())
                            {
                                editorSubsystem->SetFocusedNode(focusedNode, true);
                            }
                        })
                };
            }));

    InitObject(action);

    return action;
}

namespace /* Entity */ {

void SetEntityMaterial(Entity* entity, const Handle<Material>& material)
{
    MeshComponent* meshComponent = entity->TryGetComponent<MeshComponent>();

    if (!meshComponent)
    {
        return;
    }

    meshComponent->material = material;

    entity->SetNeedsRenderProxyUpdate();
    entity->MarkDirty();
}

bool EntityHasMesh(const Entity* entity)
{
    const MeshComponent* meshComponent = entity->TryGetComponent<MeshComponent>();

    return meshComponent != nullptr && meshComponent->mesh.IsValid();
}

} // namespace /* Entity */

namespace /* Script */ {

bool ScriptAcceptsAsset(const AssetObject* asset)
{
    return TryCast<ScriptAsset>(asset) != nullptr;
}

bool ScriptCanApply(const AssetObject* asset, const Entity* entity)
{
    return true;
}

Handle<EditorActionBase> ScriptCreateAction(EditorSubsystem* subsystem, const Handle<AssetObject>& asset, const Handle<Entity>& entity)
{
    Handle<ScriptAsset> scriptAsset = TryCast<ScriptAsset>(asset);

    const bool hadScriptComponent = entity->HasComponent<ScriptComponent>();
    const Handle<ScriptAsset> previousScriptAsset = hadScriptComponent ? entity->GetComponent<ScriptComponent>().script : Handle<ScriptAsset>::empty;

    if (previousScriptAsset == scriptAsset)
    {
        HYP_LOG(Editor, Info, "'{}' already has script '{}'", entity->GetName(), scriptAsset->GetName());

        return nullptr;
    }

    if (previousScriptAsset.IsValid())
    {
        bool shouldReplace = false;

        SystemMessageBox(MessageBoxType::WARNING)
            .Title("Replace Script")
            .Text(HYP_FORMAT("'{}' already has the script '{}' attached. Do you want to replace it with '{}'?",
                entity->GetName(), previousScriptAsset->GetName(), scriptAsset->GetName()))
            .Button("Replace", [&shouldReplace]()
                {
                    shouldReplace = true;
                })
            .Button("Cancel", []()
                {
                })
            .Show();

        if (!shouldReplace)
        {
            return nullptr;
        }
    }

    return MakeEntityDropAction(
        subsystem,
        HYP_FORMAT("Assign Script {}", scriptAsset->GetName()),
        entity,
        [entity, scriptAsset](EditorSubsystem*)
        {
            if (ScriptComponent* scriptComponent = entity->TryGetComponent<ScriptComponent>())
            {
                scriptComponent->script = scriptAsset;
            }
            else
            {
                ScriptComponent newScriptComponent;
                newScriptComponent.script = scriptAsset;
                entity->AddComponent<ScriptComponent>(std::move(newScriptComponent));
            }
        },
        [entity, previousScriptAsset, hadScriptComponent](EditorSubsystem*)
        {
            if (!hadScriptComponent)
            {
                entity->RemoveComponent<ScriptComponent>();
            }
            else if (ScriptComponent* scriptComponent = entity->TryGetComponent<ScriptComponent>())
            {
                scriptComponent->script = previousScriptAsset;
            }
        });
}

} // namespace /* Script */

namespace /* Material */ {

bool MaterialAcceptsAsset(const AssetObject* asset)
{
    return TryCast<Material>(asset) != nullptr;
}

bool MaterialCanApply(const AssetObject* asset, const Entity* entity)
{
    return EntityHasMesh(entity);
}

Handle<EditorActionBase> MaterialCreateAction(EditorSubsystem* subsystem, const Handle<AssetObject>& asset, const Handle<Entity>& entity)
{
    Handle<Material> material = TryCast<Material>(asset);

    const Handle<Material> previousMaterial = entity->GetComponent<MeshComponent>().material;

    if (previousMaterial == material)
    {
        return nullptr;
    }

    return MakeEntityDropAction(
        subsystem,
        HYP_FORMAT("Assign Material {}", material->GetName()),
        entity,
        [entity, material](EditorSubsystem*)
        {
            SetEntityMaterial(entity.Get(), material);
        },
        [entity, previousMaterial](EditorSubsystem*)
        {
            SetEntityMaterial(entity.Get(), previousMaterial);
        });
}

} // namespace /* Material */

namespace /* Texture */ {

bool TextureAcceptsAsset(const AssetObject* asset)
{
    return TryCast<Texture>(asset) != nullptr;
}

bool TextureCanApply(const AssetObject* asset, const Entity* entity)
{
    const Texture* texture = TryCast<Texture>(asset);

    return texture->GetType() == TextureType::Texture2D && EntityHasMesh(entity);
}

Handle<EditorActionBase> TextureCreateAction(EditorSubsystem* subsystem, const Handle<AssetObject>& asset, const Handle<Entity>& entity)
{
    Handle<Texture> texture = TryCast<Texture>(asset);

    const Handle<Material> previousMaterial = entity->GetComponent<MeshComponent>().material;

    if (previousMaterial.IsValid() && previousMaterial->GetTexture(MaterialTextureKey::Diffuse) == texture)
    {
        return nullptr;
    }

    Handle<Material> material = previousMaterial.IsValid()
        ? MakeHandle<Material>(NAME_FMT("{}_{}", previousMaterial->GetName(), texture->GetName()), previousMaterial)
        : MakeHandle<Material>(NAME_FMT("Material_{}", texture->GetName()));

    material->SetIsDynamic(true);
    material->SetTexture(MaterialTextureKey::Diffuse, texture);

    InitObject(material);

    return MakeEntityDropAction(
        subsystem,
        HYP_FORMAT("Assign Texture {}", texture->GetName()),
        entity,
        [entity, material](EditorSubsystem* editorSubsystem)
        {
            GetCurrentAssetRegistry()->PutAssetUnique(material);
            editorSubsystem->OnAssetsChanged(AssetBuckets::Materials.GetIndex());

            SetEntityMaterial(entity.Get(), material);
        },
        [entity, material, previousMaterial](EditorSubsystem* editorSubsystem)
        {
            SetEntityMaterial(entity.Get(), previousMaterial);

            GetCurrentAssetRegistry()->RemoveAsset(material);
            editorSubsystem->OnAssetsChanged(AssetBuckets::Materials.GetIndex());
        });
}

} // namespace /* Texture */

class FunctionEntityAssetDropHandler final : public IEntityAssetDropHandler
{
public:
    FunctionEntityAssetDropHandler(const EntityAssetDropHandler& functions)
        : m_functions(functions)
    {
    }

    bool AcceptsAsset(const AssetObject* asset) const override
    {
        return m_functions.acceptsAsset(asset);
    }

    bool CanApply(const AssetObject* asset, const Entity* entity) const override
    {
        return m_functions.canApply(asset, entity);
    }

    Handle<EditorActionBase> CreateAction(EditorSubsystem* subsystem, const Handle<AssetObject>& asset, const Handle<Entity>& entity) const override
    {
        return m_functions.createAction(subsystem, asset, entity);
    }

private:
    EntityAssetDropHandler m_functions;
};

EntityAssetDropHandlerRegistry& EntityAssetDropHandlerRegistry::GetInstance()
{
    static EntityAssetDropHandlerRegistry s_instance;

    return s_instance;
}

EntityAssetDropHandlerRegistry::EntityAssetDropHandlerRegistry()
{
    Register(MakeUniqueWithAllocator<FunctionEntityAssetDropHandler, EditorAllocator>(EntityAssetDropHandler {
        &ScriptAcceptsAsset,
        &ScriptCanApply,
        &ScriptCreateAction
    }));

    Register(MakeUniqueWithAllocator<FunctionEntityAssetDropHandler, EditorAllocator>(EntityAssetDropHandler {
        &MaterialAcceptsAsset,
        &MaterialCanApply,
        &MaterialCreateAction
    }));

    Register(MakeUniqueWithAllocator<FunctionEntityAssetDropHandler, EditorAllocator>(EntityAssetDropHandler {
        &TextureAcceptsAsset,
        &TextureCanApply,
        &TextureCreateAction
    }));

}

uint32 EntityAssetDropHandlerRegistry::Register(UniquePtr<IEntityAssetDropHandler, EditorAllocator>&& handler)
{
    Mutex::Guard guard(m_mutex);

    const uint32 id = m_nextId++;

    m_entries.PushBack(Entry { id, std::move(handler) });

    return id;
}

void EntityAssetDropHandlerRegistry::Unregister(uint32 id)
{
    Mutex::Guard guard(m_mutex);

    auto it = m_entries.FindIf([id](const Entry& entry)
        {
            return entry.id == id;
        });

    if (it != m_entries.End())
    {
        m_entries.Erase(it);
    }
}

const IEntityAssetDropHandler* EntityAssetDropHandlerRegistry::FindHandler(const AssetObject* asset) const
{
    if (!asset)
    {
        return nullptr;
    }

    Mutex::Guard guard(m_mutex);

    for (const Entry& entry : m_entries)
    {
        if (entry.handler->AcceptsAsset(asset))
        {
            return entry.handler.Get();
        }
    }

    return nullptr;
}

bool EditorEntityAssetDrop::TargetsEntity(const AssetObject* asset)
{
    return EntityAssetDropHandlerRegistry::GetInstance().FindHandler(asset) != nullptr;
}

bool EditorEntityAssetDrop::CanApplyToEntity(const AssetObject* asset, const Entity* entity)
{
    if (!entity)
    {
        return false;
    }

    const IEntityAssetDropHandler* handler = EntityAssetDropHandlerRegistry::GetInstance().FindHandler(asset);

    return handler != nullptr && handler->CanApply(asset, entity);
}

Handle<EditorActionBase> EditorEntityAssetDrop::CreateApplyAction(EditorSubsystem* subsystem, const Handle<AssetObject>& asset, const Handle<Entity>& entity)
{
    if (!CanApplyToEntity(asset.Get(), entity.Get()))
    {
        return nullptr;
    }

    return EntityAssetDropHandlerRegistry::GetInstance().FindHandler(asset.Get())->CreateAction(subsystem, asset, entity);
}

} // namespace Hyperion
