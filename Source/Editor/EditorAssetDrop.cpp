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
#include <Scene/Components/AnimationComponent.hpp>

#include <Scene/Animation/Animation.hpp>

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

namespace /* Entity */ {

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

namespace /* Animation */ {

bool AnimationAcceptsAsset(const AssetObject* asset)
{
    return TryCast<Animation>(asset) != nullptr;
}

bool AnimationCanApply(const AssetObject* asset, const Entity* entity)
{
    const MeshComponent* meshComponent = entity->TryGetComponent<MeshComponent>();

    return meshComponent != nullptr && meshComponent->skeleton.IsValid();
}

Handle<EditorActionBase> AnimationCreateAction(EditorSubsystem* subsystem, const Handle<AssetObject>& asset, const Handle<Entity>& entity)
{
    Handle<Animation> animation = TryCast<Animation>(asset);

    const bool hadAnimationComponent = entity->HasComponent<AnimationComponent>();
    const AnimationComponent previousAnimationComponent = hadAnimationComponent ? entity->GetComponent<AnimationComponent>() : AnimationComponent {};

    if (hadAnimationComponent && previousAnimationComponent.animation == animation)
    {
        return nullptr;
    }

    return MakeEntityDropAction(
        subsystem,
        HYP_FORMAT("Assign Animation {}", animation->GetName()),
        entity,
        [entity, animation](EditorSubsystem*)
        {
            if (!entity->HasComponent<AnimationComponent>())
            {
                entity->AddComponent<AnimationComponent>(AnimationComponent {});
            }

            entity->GetComponent<AnimationComponent>().Play(animation);
            entity->MarkDirty();
        },
        [entity, hadAnimationComponent, previousAnimationComponent](EditorSubsystem*)
        {
            if (!hadAnimationComponent)
            {
                entity->RemoveComponent<AnimationComponent>();
            }
            else if (AnimationComponent* animationComponent = entity->TryGetComponent<AnimationComponent>())
            {
                *animationComponent = previousAnimationComponent;
            }

            entity->MarkDirty();
        });
}

} // namespace /* Animation */

static const EntityAssetDropHandler g_entityAssetDropHandlers[] = {
    { &ScriptAcceptsAsset, &ScriptCanApply, &ScriptCreateAction },
    { &MaterialAcceptsAsset, &MaterialCanApply, &MaterialCreateAction },
    { &TextureAcceptsAsset, &TextureCanApply, &TextureCreateAction },
    { &AnimationAcceptsAsset, &AnimationCanApply, &AnimationCreateAction }
};

static const EntityAssetDropHandler* FindEntityAssetDropHandler(const AssetObject* asset)
{
    if (!asset)
    {
        return nullptr;
    }

    for (const EntityAssetDropHandler& handler : g_entityAssetDropHandlers)
    {
        if (handler.acceptsAsset(asset))
        {
            return &handler;
        }
    }

    return nullptr;
}

bool EditorEntityAssetDrop::TargetsEntity(const AssetObject* asset)
{
    return FindEntityAssetDropHandler(asset) != nullptr;
}

bool EditorEntityAssetDrop::CanApplyToEntity(const AssetObject* asset, const Entity* entity)
{
    if (!entity)
    {
        return false;
    }

    const EntityAssetDropHandler* handler = FindEntityAssetDropHandler(asset);

    return handler != nullptr && handler->canApply(asset, entity);
}

Handle<EditorActionBase> EditorEntityAssetDrop::CreateApplyAction(EditorSubsystem* subsystem, const Handle<AssetObject>& asset, const Handle<Entity>& entity)
{
    if (!CanApplyToEntity(asset.Get(), entity.Get()))
    {
        return nullptr;
    }

    return FindEntityAssetDropHandler(asset.Get())->createAction(subsystem, asset, entity);
}

} // namespace Hyperion
