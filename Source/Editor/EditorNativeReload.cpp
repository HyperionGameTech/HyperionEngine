/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#include <EditorPch.hpp>

#include <Editor/EditorNativeReload.hpp>
#include <Editor/EditorProject.hpp>
#include <Editor/EditorState.hpp>
#include <Editor/EditorSubsystem.hpp>
#include <Editor/EditorActionStack.hpp>
#include <Editor/Csg/EditorCsgState.hpp>
#include <Editor/Prefab/EditorPrefabEditState.hpp>

#include <Framework/Game.hpp>
#include <Framework/EngineGlobals.hpp>

#include <Asset/AssetObject.hpp>
#include <Asset/AssetRegistry.hpp>
#include <Asset/SerializationUtils.hpp>

#include <Scene/World.hpp>
#include <Scene/Scene.hpp>
#include <Scene/Node.hpp>
#include <Scene/Entity.hpp>
#include <Scene/EntityManager.hpp>
#include <Scene/ComponentContainer.hpp>
#include <Scene/ComponentInterface.hpp>
#include <Scene/System.hpp>
#include <Scene/Subsystem.hpp>

#include <Core/Reflection/Class.hpp>
#include <Core/Reflection/ClassRegistry.hpp>
#include <Core/Reflection/Struct.hpp>
#include <Core/Reflection/Property.hpp>

#include <Core/Utilities/GlobalContext.hpp>

#include <Core/Logging/Logger.hpp>

namespace Hyperion {

EDITOR_API HYP_DECLARE_LOG_CHANNEL(Editor);

namespace {

struct RebuiltObject
{
    Handle<ObjectBase> oldObject;
    Handle<ObjectBase> newObject;
};

bool DerivesFromRetiredClass(const Class* cls, const Array<const Class*>& retiredClasses)
{
    for (; cls != nullptr; cls = cls->GetParent())
    {
        if (retiredClasses.Contains(cls))
        {
            return true;
        }
    }

    return false;
}

// the new build's class for an object of a retired class, or null when the object stays on the old build
const Class* GetRebuildClass(const Class* oldClass, Name objectName)
{
    if (oldClass->IsDynamic())
    {
        HYP_LOG(Editor, Warning, "{} is a C# class deriving from a C++ game class; {} stays on the previous game module build", oldClass->GetName(), objectName);

        return nullptr;
    }

    const Class* newClass = GetClass(oldClass->GetTypeId());

    if (newClass == nullptr || newClass == oldClass)
    {
        HYP_LOG(Editor, Warning, "{} is gone from the new game module build; {} stays on the previous build until it's deleted", oldClass->GetName(), objectName);

        return nullptr;
    }

    return newClass;
}

const Handle<ObjectBase>* FindRebuilt(const ObjectBase* object, const Array<RebuiltObject>& rebuiltObjects)
{
    if (object == nullptr)
    {
        return nullptr;
    }

    for (const RebuiltObject& rebuiltObject : rebuiltObjects)
    {
        if (rebuiltObject.oldObject.Get() == object)
        {
            return &rebuiltObject.newObject;
        }
    }

    return nullptr;
}

Array<Scene*> GetProjectScenes(EditorProject* project)
{
    Array<Scene*> scenes;

    if (const Handle<World>& world = project->GetWorld(); world.IsValid())
    {
        for (const Handle<Scene>& scene : world->GetScenes())
        {
            if (scene.IsValid() && scene->GetRoot().IsValid())
            {
                scenes.PushBack(scene.Get());
            }
        }
    }

    return scenes;
}

Array<Handle<AssetObject>> GetCachedAssets(EditorProject* project)
{
    Array<Handle<AssetObject>> assets;

    if (const Handle<Game>& game = project->GetGame(); game.IsValid() && game->GetAssetRegistry().IsValid())
    {
        game->GetAssetRegistry()->GetCachedAssets(assets);
    }

    return assets;
}

// a system can be saved with its World, so it's replaced by the new build's rather than just dropped
uint32 ReplaceRetiredSystems(World& world, const Array<const Class*>& retiredClasses, Array<RebuiltObject>& rebuiltObjects)
{
    uint32 numReplaced = 0;

    Array<Handle<SystemBase>> oldSystems;

    for (const Handle<SystemBase>& system : world.GetSystems())
    {
        if (system.IsValid() && DerivesFromRetiredClass(system->InstanceClass(), retiredClasses))
        {
            oldSystems.PushBack(system);
        }
    }

    for (const Handle<SystemBase>& oldSystem : oldSystems)
    {
        const Class* oldClass = oldSystem->InstanceClass();
        const Class* newClass = GetRebuildClass(oldClass, oldClass->GetName());

        Handle<SystemBase> newSystem;

        if (BoxedValue boxed; newClass != nullptr && newClass->CreateInstance(boxed) && boxed.Is<Handle<SystemBase>>())
        {
            newSystem = boxed.Get<Handle<SystemBase>>();

            BoxedValue source { Handle<ObjectBase>(oldSystem) };
            BoxedValue target { Handle<ObjectBase>(newSystem) };

            CopyMembersByName(oldClass, source, newClass, target, /* stopAtClass */ SystemBase::StaticClass());
        }

        world.RemoveSystem(oldSystem.Get());

        if (newSystem.IsValid())
        {
            world.AddSystem(newSystem);

            rebuiltObjects.PushBack({ oldSystem, newSystem });
            numReplaced++;
        }
    }

    Array<Handle<Subsystem>> oldSubsystems;

    for (Subsystem* subsystem : world.GetSubsystems())
    {
        if (subsystem != nullptr && DerivesFromRetiredClass(subsystem->InstanceClass(), retiredClasses))
        {
            oldSubsystems.PushBack(MakeStrongRef(subsystem));
        }
    }

    for (const Handle<Subsystem>& oldSubsystem : oldSubsystems)
    {
        const Class* oldClass = oldSubsystem->InstanceClass();
        const Class* newClass = GetRebuildClass(oldClass, oldClass->GetName());

        Handle<Subsystem> newSubsystem;

        if (BoxedValue boxed; newClass != nullptr && newClass->CreateInstance(boxed) && boxed.Is<Handle<Subsystem>>())
        {
            newSubsystem = boxed.Get<Handle<Subsystem>>();

            BoxedValue source { Handle<ObjectBase>(oldSubsystem) };
            BoxedValue target { Handle<ObjectBase>(newSubsystem) };

            CopyMembersByName(oldClass, source, newClass, target, /* stopAtClass */ Subsystem::StaticClass());
        }

        world.RemoveSubsystem(oldSubsystem.Get());

        // keyed by TypeId, which both builds share, so the old one has to go first
        if (newSubsystem.IsValid())
        {
            world.AddSubsystem(newClass->GetTypeId(), newSubsystem);

            rebuiltObjects.PushBack({ oldSubsystem, newSubsystem });
            numReplaced++;
        }
    }

    return numReplaced;
}

Handle<Node> RebuildNode(const Handle<Node>& oldNode, const Class* newClass)
{
    Node* parent = oldNode->GetParent();

    if (parent == nullptr)
    {
        HYP_LOG(Editor, Warning, "{} has no parent, so it stays on the previous game module build", oldNode->GetName());

        return Handle<Node>::Null();
    }

    Handle<Node> newNode = oldNode->CloneAs(newClass, /* cloneChildren */ false);

    if (!newNode.IsValid())
    {
        HYP_LOG(Editor, Error, "Failed to rebuild {} as {}; it stays on the previous game module build", oldNode->GetName(), newClass->GetName());

        return Handle<Node>::Null();
    }

    newNode->SetUUID(oldNode->GetUUID());

    // added before the old node goes, so anything keyed by UUID ends up on the new one
    parent->InsertChild(newNode, oldNode->FindSelfIndex());

    Array<Handle<Node>> children;

    for (Node* child : oldNode->GetChildren())
    {
        children.PushBack(MakeStrongRef(child));
    }

    for (const Handle<Node>& child : children)
    {
        child->Remove();

        newNode->AddChild(child);
    }

    oldNode->Remove();

    return newNode;
}

// put in the old asset's place in the registry, so its name, path and index carry over and the old one is evicted
Handle<AssetObject> RebuildAsset(AssetRegistry& registry, const Handle<AssetObject>& oldAsset, const Class* newClass)
{
    BoxedValue boxed;

    if (!newClass->CreateInstance(boxed) || !boxed.Is<Handle<AssetObject>>())
    {
        HYP_LOG(Editor, Error, "Failed to rebuild asset {} as {}; it stays on the previous game module build", oldAsset->GetName(), newClass->GetName());

        return Handle<AssetObject>::Null();
    }

    Handle<AssetObject> newAsset = boxed.Get<Handle<AssetObject>>();

    BoxedValue source { Handle<ObjectBase>(oldAsset) };
    BoxedValue target { Handle<ObjectBase>(newAsset) };

    CopyMembersByName(oldAsset->InstanceClass(), source, newClass, target, /* stopAtClass */ AssetObject::StaticClass());

    newAsset->SetUUID(oldAsset->GetUUID());
    newAsset->SetName(oldAsset->GetName());
    newAsset->SetAssetFlags(oldAsset->GetAssetFlags());

    registry.PutAsset(newAsset);

    return newAsset;
}

// calls back with each reflected handle member of the object, and of its components for an entity, with the value it's on
template <class Callback>
void ForEachHandleMember(ObjectBase* object, Callback&& callback)
{
    BoxedValue target(MakeStrongRef(object));

    for (const Class* cls = object->InstanceClass(); cls != nullptr; cls = cls->GetParent())
    {
        for (const IMember& member : cls->GetMembers(MemberType::Field | MemberType::Property, /* deep */ false))
        {
            if (member.GetTypeInfo().IsHandleType() && CanGetAndSetMember(member))
            {
                callback(member, target);
            }
        }
    }

    Entity* entity = TryCast<Entity>(object);
    EntityManager* entityManager = entity != nullptr ? entity->GetEntityManager() : nullptr;

    if (entityManager == nullptr)
    {
        return;
    }

    Optional<const ComponentMap&> components = entityManager->GetAllComponents(entity);

    if (!components.HasValue())
    {
        return;
    }

    for (const auto& it : *components)
    {
        ComponentContainer* container = entityManager->TryGetContainer(it.first);

        if (container == nullptr || container->GetComponentInterface().IsEntityTag())
        {
            continue;
        }

        AnyRef componentRef = container->TryGetComponent(it.second);

        if (!componentRef.HasValue())
        {
            continue;
        }

        BoxedValue component(componentRef);

        for (const Property* property : container->GetComponentInterface().GetStruct()->GetProperties())
        {
            if (property->GetTypeInfo().IsHandleType() && CanGetAndSetMember(*property))
            {
                callback(*property, component);
            }
        }
    }
}

// reflected handles to a rebuilt object are pointed at its new object
uint32 RetargetHandles(ObjectBase* object, const Array<RebuiltObject>& rebuiltObjects)
{
    uint32 numRetargeted = 0;

    ForEachHandleMember(object, [&rebuiltObjects, &numRetargeted](const IMember& member, BoxedValue& target)
        {
            const BoxedValue value = GetMemberValue(member, target);

            if (!value.Is<Handle<ObjectBase>>())
            {
                return;
            }

            if (const Handle<ObjectBase>* replacement = FindRebuilt(value.Get<Handle<ObjectBase>>().Get(), rebuiltObjects))
            {
                SetMemberValue(member, target, BoxedValue(*replacement));

                numRetargeted++;
            }
        });

    return numRetargeted;
}

// old objects that refer to each other would keep each other, and the old build, alive
void ClearHandlesToRebuiltObjects(ObjectBase* oldObject, const Array<RebuiltObject>& rebuiltObjects)
{
    ForEachHandleMember(oldObject, [&rebuiltObjects](const IMember& member, BoxedValue& target)
        {
            const BoxedValue value = GetMemberValue(member, target);

            if (value.Is<Handle<ObjectBase>>() && FindRebuilt(value.Get<Handle<ObjectBase>>().Get(), rebuiltObjects) != nullptr)
            {
                SetMemberValue(member, target, BoxedValue());
            }
        });
}

Handle<Node> Remap(const Handle<Node>& node, const Array<RebuiltObject>& rebuiltObjects)
{
    const Handle<ObjectBase>* replacement = FindRebuilt(node.Get(), rebuiltObjects);

    return replacement != nullptr ? TryCast<Node>(*replacement) : node;
}

void RemapEditorReferences(EditorSubsystem* subsystem, EditorProject* project, const Array<RebuiltObject>& rebuiltObjects)
{
    if (subsystem)
    {
        const Handle<Node> focusedNode = subsystem->GetFocusedNode();

        Array<Handle<Node>> selectedNodes = subsystem->GetSelectedNodes();

        for (Handle<Node>& selectedNode : selectedNodes)
        {
            selectedNode = Remap(selectedNode, rebuiltObjects);
        }

        subsystem->SetSelectedNodes(selectedNodes);

        if (FindRebuilt(focusedNode.Get(), rebuiltObjects) != nullptr)
        {
            subsystem->SetFocusedNode(Remap(focusedNode, rebuiltObjects), /* shouldSelectInOutline */ false);
        }
    }

    if (g_editorState.IsValid())
    {
        Array<Handle<Node>> clipboardNodes = g_editorState->GetClipboardNodes();

        for (Handle<Node>& clipboardNode : clipboardNodes)
        {
            clipboardNode = Remap(clipboardNode, rebuiltObjects);
        }

        g_editorState->SetClipboardNodes(clipboardNodes);
    }

    // undo closures hold the old objects and can't be remapped
    Proc<bool(EditorActionBase*)> removeAllActions = [](EditorActionBase*)
    {
        return true;
    };

    project->GetActionStack()->RemoveActions(removeAllActions);
}

} // namespace

EDITOR_API void RebuildRetiredModuleObjects(EditorProject* project, EditorSubsystem* subsystem, const Array<const Class*>& retiredClasses)
{
    if (!project || retiredClasses.Empty())
    {
        return;
    }

    if (subsystem)
    {
        subsystem->ExitIsolatedEditStates(/* apply */ true);

        subsystem->ExitMeshEditMode(/* saveEdits */ true);
        subsystem->GetCsgState()->Exit(/* saveEdits */ true);
    }

    Array<RebuiltObject> rebuiltObjects;
    uint32 numRebuiltNodes = 0;
    uint32 numRebuiltAssets = 0;
    uint32 numReplacedSystems = 0;

    Array<DeferredHandleCopy> deferredCopies;
    GlobalContextScope deferredCopiesScope(DeferredHandleCopiesContext { &deferredCopies });

    const Array<Scene*> scenes = GetProjectScenes(project);

    // components move to the new build's layouts first: new systems see them, and a rebuilt entity takes its old components with it
    for (Scene* scene : scenes)
    {
        scene->GetEntityManager()->SyncRuntimeComponentTypes();
    }

    // before the nodes are rebuilt, so new entities that add their systems find the new ones
    if (const Handle<World>& world = project->GetWorld(); world.IsValid())
    {
        numReplacedSystems = ReplaceRetiredSystems(*world, retiredClasses, rebuiltObjects);
    }

    if (const Handle<Game>& game = project->GetGame(); game.IsValid() && game->GetAssetRegistry().IsValid())
    {
        for (const Handle<AssetObject>& oldAsset : GetCachedAssets(project))
        {
            if (!DerivesFromRetiredClass(oldAsset->InstanceClass(), retiredClasses))
            {
                continue;
            }

            const Class* newClass = GetRebuildClass(oldAsset->InstanceClass(), oldAsset->GetName());

            if (newClass == nullptr)
            {
                continue;
            }

            if (Handle<AssetObject> newAsset = RebuildAsset(*game->GetAssetRegistry(), oldAsset, newClass); newAsset.IsValid())
            {
                rebuiltObjects.PushBack({ oldAsset, std::move(newAsset) });
                numRebuiltAssets++;
            }
        }
    }

    Array<Handle<Node>> retiredNodes;

    for (Scene* scene : scenes)
    {
        // parents come before their children
        for (Node* node : scene->GetRoot()->GetDescendantsArray())
        {
            if (DerivesFromRetiredClass(node->InstanceClass(), retiredClasses))
            {
                retiredNodes.PushBack(MakeStrongRef(node));
            }
        }
    }

    for (const Handle<Node>& oldNode : retiredNodes)
    {
        const Class* newClass = GetRebuildClass(oldNode->InstanceClass(), oldNode->GetName());

        if (!newClass)
        {
            continue;
        }

        if (Handle<Node> newNode = RebuildNode(oldNode, newClass); newNode.IsValid())
        {
            rebuiltObjects.PushBack({ oldNode, std::move(newNode) });
            numRebuiltNodes++;
        }
    }

    if (rebuiltObjects.Empty())
    {
        return;
    }

    uint32 numRetargeted = 0;

    for (DeferredHandleCopy& deferredCopy : deferredCopies)
    {
        const Handle<ObjectBase>* replacement = FindRebuilt(deferredCopy.value.Get(), rebuiltObjects);

        if (!replacement)
        {
            HYP_LOG(Editor, Warning, "{} refers to a {} that stays on the previous game module build",
                deferredCopy.member->GetName(), deferredCopy.value->InstanceClass()->GetName());

            continue;
        }

        SetMemberValue(
            *deferredCopy.member,
            deferredCopy.target,
            BoxedValue(*replacement));

        numRetargeted++;
    }

    deferredCopies.Clear();

    for (Scene* scene : scenes)
    {
        for (Node* node : scene->GetRoot()->GetDescendantsArray())
        {
            if (!DerivesFromRetiredClass(node->InstanceClass(), retiredClasses))
            {
                numRetargeted += RetargetHandles(node, rebuiltObjects);
            }
        }
    }

    for (const Handle<AssetObject>& asset : GetCachedAssets(project))
    {
        if (!DerivesFromRetiredClass(asset->InstanceClass(), retiredClasses))
        {
            numRetargeted += RetargetHandles(asset.Get(), rebuiltObjects);
        }
    }

    {
        // the old objects are read and written through their own build's code
        ClassLookupOverrideScope lookupOverride(retiredClasses);

        for (const RebuiltObject& rebuiltObject : rebuiltObjects)
        {
            ClearHandlesToRebuiltObjects(rebuiltObject.oldObject.Get(), rebuiltObjects);
        }
    }

    RemapEditorReferences(subsystem, project, rebuiltObjects);

    // the inspector rebuilds its view of the selection the same way as for a script reload
    if (subsystem)
    {
        subsystem->OnScriptReloaded();
    }

    HYP_LOG(Editor, Info, "Rebuilt {} node(s), {} asset(s) and {} system(s) of native game classes on the new build ({} reference(s) updated). Undo history has been cleared.",
        numRebuiltNodes, numRebuiltAssets, numReplacedSystems, numRetargeted);
}

} // namespace Hyperion
