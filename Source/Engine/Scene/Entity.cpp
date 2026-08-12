/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#include <ScenePch.hpp>

#include <Scene/Entity.hpp>
#include <Scene/Scene.hpp>
#include <Scene/World.hpp>
#include <Scene/Node.hpp>
#include <Scene/DetachedScene.hpp>

#include <Scene/EntityManager.hpp>
#include <Scene/EntityTag.hpp>

#include <Scene/ComponentInterface.hpp>
#include <Scene/Components/MeshComponent.hpp>
#include <Scene/Components/ScriptComponent.hpp>
#include <Scene/Components/TransformComponent.hpp>
#include <Scene/Components/VisibilityStateComponent.hpp>
#include <Scene/Components/BoundingBoxComponent.hpp>
#include <Scene/Components/LightmapElementComponent.hpp>

#include <Scripting/EntityScripting.hpp>

#include <Rendering/Mesh.hpp>
#include <Rendering/Material.hpp>
#include <Rendering/RenderProxy.hpp>
#include <Rendering/InstancedMeshData.hpp>

#include <Framework/EngineDriver.hpp>

#include <Asset/AssetObject.hpp>
#include <Asset/AssetRegistry.hpp>
#include <Asset/SerializationUtils.hpp>

#include <Entity.generated.inl>

namespace Hyperion {

#pragma region Entity

Entity::Entity()
    : Entity(Name::Invalid())
{
}

Entity::Entity(Name name)
    : Node(name),
      m_entityManager(nullptr),
      m_renderProxyVersion(0),
      m_transformChanged(false)
{
}

Entity::~Entity()
{
    EntityManager* entityManager = GetEntityManager();
    if (entityManager == nullptr)
    {
        return;
    }

    // Can only be destroyed if no EM exists, or we are on the EM's owner thread.
    Assert(entityManager->IsDetachedScene() || IsOnThread(entityManager->GetOwnerThreadId()), "Destroying Entity {} from wrong thread while still attached to EntityManager!", GetName());

    HYP_LOG(Entity, Verbose, "Removing Entity {} from entity manager", GetName());

    if (!entityManager->RemoveEntity(this, /* calledFromEntityDestructor */ true))
    {
        HYP_LOG(Entity, Error, "Failed to remove Entity {} from EntityManager", GetName());
    }

    m_entityManager = nullptr;
}

Handle<Node> Entity::Clone() const
{
    // Clone Node base
    Handle<Node> baseClone = Node::Clone();
    if (!baseClone.IsValid())
    {
        return Handle<Node>::Null();
    }

    // baseClone would be an Entity because it uses InstanceClass()
    // to create an instance based on the runtime type of this
    Handle<Entity> cloned = DynamicCast<Entity>(baseClone);
    AssertDebug(cloned.IsValid());

    if (!cloned.IsValid())
    {
        HYP_LOG(Entity, Error, "Base clone is not an Entity");
        return baseClone;
    }

    cloned->m_entityInitInfo = m_entityInitInfo;

    InitObject(cloned);

    // Copy components of this
    EntityManager* entityManager = GetEntityManager();
    if (entityManager != nullptr && m_scene != nullptr)
    {
        Array<BoxedValue, DynamicAllocator> serializedComponents = SerializeComponents();

        cloned->DeserializeComponents(serializedComponents);

        // Copy serializable entity tags (skip runtime-only tags like FocusedInEditor)
        Array<EntityTag> serializedTags = SerializeTags();

        cloned->DeserializeTags(serializedTags);
    }

    return cloned;
}

void Entity::Init()
{
    AssertDebug(m_scene != nullptr);
    SetEntityManager(m_scene->GetEntityManager());

    Node::Init();

    SetReady(true);
}

bool Entity::ReceivesUpdate() const
{
    if (!m_entityInitInfo.canEverUpdate)
    {
        return false;
    }

    EntityManager* entityManager = GetEntityManager();
    AssertDebug(entityManager != nullptr, "EntityManager is null for Entity {} while checking receives update", Id());

    AssertOnThread(entityManager->GetOwnerThreadId());

    return entityManager->HasTag<EntityTag::ReceivesUpdate>(this);
}

void Entity::SetReceivesUpdate(bool receivesUpdate)
{
    if (!m_entityInitInfo.canEverUpdate)
    {
        AssertDebug(!receivesUpdate, "Entity {} cannot receive updates, but SetReceivesUpdate() was called with true", Id());

        return;
    }

    EntityManager* entityManager = GetEntityManager();

    if (!entityManager)
    {
        m_entityInitInfo.receivesUpdate = receivesUpdate;

        return;
    }

    AssertOnThread(entityManager->GetOwnerThreadId());

    if (receivesUpdate)
    {
        entityManager->AddTag<EntityTag::ReceivesUpdate>(this);
    }
    else
    {
        entityManager->RemoveTag<EntityTag::ReceivesUpdate>(this);
    }
}

void Entity::OnAttachedToNode(Node* node)
{
    Node::OnAttachedToNode(node);

    // SetScene() should've been called before this,
    // so EntityManager should be updated
    AssertDebug(GetEntityManager() == node->GetScene()->GetEntityManager());
}

void Entity::OnNodeAttached(Node* node)
{
    // needs world bounds update when a child is attached
    if (EntityManager* entityManager = GetEntityManager())
    {
        BoundingBoxComponent& boundingBoxComponent = entityManager->GetComponent<BoundingBoxComponent>(this);
        boundingBoxComponent.worldAabb = GetWorldBounds();
    }
}

void Entity::OnNodeDetached(Node* node)
{
    // needs world bounds update when a child is detached
    if (EntityManager* entityManager = GetEntityManager())
    {
        BoundingBoxComponent& boundingBoxComponent = entityManager->GetComponent<BoundingBoxComponent>(this);
        boundingBoxComponent.worldAabb = GetWorldBounds();
    }
}

void Entity::OnDetachedFromNode(Node* node)
{
    Node::OnDetachedFromNode(node);
}

void Entity::OnAddedToWorld(World* world)
{
    EntityManager* entityManager = GetEntityManager();

    if (entityManager)
    {
        entityManager->AddTag<EntityTag::UpdateVisibility>(this);
    }
}

void Entity::OnRemovedFromWorld(World* world)
{
}

void Entity::OnAddedToScene(Scene* scene)
{
    AssertDebug(scene != nullptr);

    // If a TransformComponent already exists on the Entity, allow it to keep its current transform by moving the Node
    // to match it, as long as we're not locked
    // If transform is locked, the Entity's TransformComponent will be synced with the Node's current transform
    if (TransformComponent* transformComponent = m_entityManager->TryGetComponent<TransformComponent>(this))
    {
        transformComponent->translation = GetWorldTranslation();
        transformComponent->rotation = GetWorldRotation();
        transformComponent->scale = GetWorldScale();
    }
    else
    {
        m_entityManager->AddComponent<TransformComponent>(this, TransformComponent { GetWorldTranslation(), GetWorldRotation(), GetWorldScale() });
    }

    if (BoundingBoxComponent* boundingBoxComponent = m_entityManager->TryGetComponent<BoundingBoxComponent>(this))
    {
        boundingBoxComponent->worldAabb = GetWorldBounds();
    }
    else
    {
        m_entityManager->AddComponent<BoundingBoxComponent>(this, BoundingBoxComponent { GetWorldBounds() });
    }

    if (!m_entityManager->HasComponent<VisibilityStateComponent>(this))
    {
        m_entityManager->AddComponent<VisibilityStateComponent>(this, {});
    }

    m_entityManager->AddTags<EntityTag::UpdateVisibility, EntityTag::UpdateRenderProxy>(this);

    if (IsStatic())
    {
        m_entityManager->RemoveTag<EntityTag::MobDynamic>(this);
        m_entityManager->AddTag<EntityTag::MobStatic>(this);
    }
    else
    {
        m_entityManager->RemoveTag<EntityTag::MobStatic>(this);
        m_entityManager->AddTag<EntityTag::MobDynamic>(this);
    }

    m_transformChanged = false;
}

void Entity::OnRemovedFromScene(Scene* scene)
{
    AssertDebug(scene != nullptr);

    if (EntityManager* entityManager = GetEntityManager())
    {
        VisibilityStateComponent& visibilityStateComponent = entityManager->GetComponent<VisibilityStateComponent>(this);
        visibilityStateComponent.octantId = OctantId::Invalid();
        visibilityStateComponent.visibilityState = nullptr;
    }
}

void Entity::OnComponentAdded(AnyRef component)
{
#ifdef HYP_EDITOR
    if (const Class* componentClass = component.GetClass(); componentClass != nullptr && componentClass->CanSerialize())
    {
        MarkDirty();
    }
#endif // HYP_EDITOR

    if (MeshComponent* meshComponent = component.TryGet<MeshComponent>())
    {
        bool isInvalid = false;

        if (!meshComponent->mesh.IsValid())
        {
            HYP_LOG(Entity, Warning, "Entity {} has a MeshComponent with an invalid mesh", Id());

            isInvalid = true;
        }

        if (!meshComponent->material.IsValid())
        {
            HYP_LOG(Entity, Warning, "Entity {} has a MeshComponent with an invalid material", Id());

            isInvalid = true;
        }

        InitObject(meshComponent->mesh);
        InitObject(meshComponent->material);

        if (isInvalid)
        {
            return;
        }

        AddTag<EntityTag::UpdateRenderProxy>();

#ifdef HYP_EDITOR
        // build mesh BVH if there is no existing one. (size != 0)
        if (m_entityInitInfo.bvhDepth > 0 && meshComponent->mesh->GetBVHDataReference().size == 0)
        {
            meshComponent->mesh->RebuildBVH();
        }
#endif // HYP_EDITOR

        return;
    }
}

void Entity::OnComponentRemoved(AnyRef component)
{
#ifdef HYP_EDITOR
    if (const Class* componentClass = component.GetClass(); componentClass != nullptr && componentClass->CanSerialize())
    {
        MarkDirty();
    }
#endif // HYP_EDITOR
}

void Entity::OnTagAdded(EntityTag tag)
{
    const bool isSerializableTag = (uint64(tag) & EntityTag::SerializableTagMask) != 0;

#ifdef HYP_EDITOR
    if (isSerializableTag)
    {
        MarkDirty();
    }
#endif // HYP_EDITOR

    // So we update the octant's hash code.
    if (isSerializableTag && m_entityManager)
    {
        m_entityManager->AddTag<EntityTag::UpdateVisibility>(this);
    }
}

void Entity::OnTagRemoved(EntityTag tag)
{
    const bool isSerializableTag = (uint64(tag) & EntityTag::SerializableTagMask) != 0;

#ifdef HYP_EDITOR
    if (isSerializableTag)
    {
        MarkDirty();
    }
#endif // HYP_EDITOR

    // So we update the octant's hash code.
    if (isSerializableTag && m_entityManager)
    {
        m_entityManager->AddTag<EntityTag::UpdateVisibility>(this);
    }
}

void Entity::SetScene_Internal(Scene* scene, bool moveToDetached)
{
    EntityManager* prevEntityManager = GetEntityManager();

    // We need to call RemoveEntity() if MoveEntity() will not be called.
    // Do this before Node::SetScene_Internal() is called, because systems may try to do
    // entity->GetScene() and will not expect nullptr to be returned.
    if (scene == nullptr && !moveToDetached && prevEntityManager != nullptr)
    {
        prevEntityManager->RemoveEntity(this);
    }

    Node::SetScene_Internal(scene, moveToDetached);

    if (m_scene != nullptr)
    {
        SetEntityManager(m_scene->GetEntityManager());
    }
    else
    {
        m_entityManager = nullptr;
    }
}

void Entity::UpdateRenderProxy(RenderProxyMesh* proxy)
{
    /// Must have a MeshComponent if this is called.
    MeshComponent& meshComponent = GetComponent<MeshComponent>();
    TransformComponent& transformComponent = GetComponent<TransformComponent>();

    LightmapElementComponent* lightmapElementComponent = TryGetComponent<LightmapElementComponent>();

    proxy->forceRebind = false;
    proxy->entity = this;
    proxy->mesh = meshComponent.mesh;
    proxy->material = meshComponent.material;
    proxy->skeleton = meshComponent.skeleton;
    proxy->numIndices = meshComponent.mesh->NumIndices(proxy->currentLodIndex);
    proxy->numInstances = meshComponent.numInstances;
    proxy->enableAutoInstancing = meshComponent.enableAutoInstancing;
    proxy->attributes = RenderableAttributeSet(meshComponent.mesh->GetMeshAttributes(), meshComponent.material->GetAttributes());

    if (lightmapElementComponent != nullptr)
    {
        proxy->lightmapVolume = lightmapElementComponent->lightmapVolume.GetUnsafe();
        proxy->lightmapElementId = lightmapElementComponent->lightmapElementId;
    }
    else
    {
        proxy->lightmapVolume = nullptr;
        proxy->lightmapElementId = Invalid<LightmapElementId>;
    }

    Mat4f transformMatrix = transformComponent.GetMatrix();

    if (meshComponent.enableAutoInstancing || meshComponent.numInstances)
    {
        AssertDebug(meshComponent.instanceData.IsLoaded());

        const Handle<InstancedMeshData>& imd = DynamicCast<InstancedMeshData>(meshComponent.instanceData.Resolve());
        AssertDebug(imd.IsValid());

        if (imd.IsValid())
        {
            auto scope = imd->GetReadScope();

            for (uint32 i = 0; i < uint32(imd->buffers.Size()); i++)
            {
                if (imd->buffers[i].size == 0)
                {
                    continue;
                }

                proxy->instanceData.buffers[i].SetSize(imd->buffers[i].size, false);

                AssertDebug(imd->buffers[i].raw != nullptr);
                Memory::Copy(proxy->instanceData.buffers[i].Data(), imd->buffers[i].raw, imd->buffers[i].size);

                proxy->instanceData.bufferStructSizes[i] = imd->bufferStructSizes[i];
                proxy->instanceData.bufferStructAlignments[i] = imd->bufferStructAlignments[i];
            }
        }
    }
    else
    {
        proxy->instanceData = {};
    }

    const BoundingBox meshWorldBounds = transformMatrix * proxy->mesh->GetAABB();
    proxy->bufferData.worldAabbMax = meshWorldBounds.max;
    proxy->bufferData.worldAabbMin = meshWorldBounds.min;

    proxy->bufferData.modelMatrix = transformMatrix;
    proxy->bufferData.previousModelMatrix = meshComponent.previousModelMatrix;
    proxy->bufferData.normalMatrix = Mat3f(transformMatrix).Inverse().Transpose();
    proxy->bufferData.bucket = uint32(meshComponent.material->GetAttributes().bucket);
}

void Entity::LockTransform()
{
    Node::LockTransform();

    if (IsInitCalled())
    {
        EntityManager* entityManager = GetEntityManager();
        AssertDebug(entityManager != nullptr);

        m_transformChanged = false;
    }
}

void Entity::UnlockTransform()
{
    Node::UnlockTransform();
}

void Entity::SetLocalBounds(const BoundingBox& aabb)
{
    Node::SetLocalBounds(aabb);

    if (EntityManager* entityManager = GetEntityManager())
    {
        BoundingBoxComponent& boundingBoxComponent = entityManager->GetComponent<BoundingBoxComponent>(this);
        boundingBoxComponent.worldAabb = GetWorldBounds();

        SetNeedsRenderProxyUpdate();

        entityManager->AddTags<
            EntityTag::UpdateVisibility,
            EntityTag::UpdateRenderProxy>(this);
    }
}

void Entity::OnTransformUpdated()
{
    Node::OnTransformUpdated();

    if (!IsInitCalled())
    {
        return;
    }

    EntityManager* entityManager = GetEntityManager();

    if (!entityManager)
    {
        return;
    }

    AssertDebug(entityManager == m_scene->GetEntityManager());

    if (!m_transformChanged)
    {
        m_transformChanged = true;
    }

    TransformComponent& transformComponent = entityManager->GetComponent<TransformComponent>(this);
    transformComponent.translation = GetWorldTranslation();
    transformComponent.rotation = GetWorldRotation();
    transformComponent.scale = GetWorldScale();

    BoundingBoxComponent& boundingBoxComponent = entityManager->GetComponent<BoundingBoxComponent>(this);
    boundingBoxComponent.worldAabb = GetWorldBounds();

    entityManager->AddTags<
        EntityTag::UpdateVisibility,
        EntityTag::UpdateRenderProxy>(this);
}

void Entity::OnMobilityChanged(bool isStatic)
{
    Node::OnMobilityChanged(isStatic);

    if (!IsInitCalled())
    {
        return;
    }

    EntityManager* entityManager = GetEntityManager();
    AssertDebug(entityManager != nullptr);
    AssertDebug(entityManager == m_scene->GetEntityManager());

    if (isStatic)
    {
        entityManager->RemoveTag<EntityTag::MobDynamic>(this);
        entityManager->AddTag<EntityTag::MobStatic>(this);
    }
    else
    {
        entityManager->RemoveTag<EntityTag::MobStatic>(this);
        entityManager->AddTag<EntityTag::MobDynamic>(this);
    }

    AddTag<EntityTag::UpdateRenderProxy>();
}

void Entity::SetEntityManager(const Handle<EntityManager>& entityManager)
{
    AssertDebug(entityManager != nullptr);

    EntityManager* previousEntityManager = GetEntityManager();

    if (previousEntityManager)
    {
        if (previousEntityManager != entityManager)
        {
            previousEntityManager->MoveEntity(MakeStrongRef(this), entityManager);
        }
    }
    else
    {
        entityManager->AddExistingEntity(MakeStrongRef(this));
    }

    AssertDebug(m_entityManager == entityManager);
}

static bool ShouldSkipEntityTagForSerialization(EntityTag tag)
{
    return tag == EntityTag::None
        || tag == EntityTag::MobStatic
        || tag == EntityTag::MobDynamic;
}

Array<EntityTag> Entity::SerializeTags() const
{
    EntityManager* entityManager = GetEntityManager();

    if (!entityManager)
    {
        return {};
    }

    Array<EntityTag> resultTags;

    auto SerializeEntityTags = [this, entityManager, &resultTags]()
    {
        Optional<const ComponentMap&> allComponentsOpt = entityManager->GetAllComponents(this);

        if (!allComponentsOpt.HasValue())
        {
            return;
        }

        for (const auto& it : *allComponentsOpt)
        {
            const IComponentInterface* componentInterface = ComponentInterfaceRegistry::GetInstance().GetComponentInterface(it.first);

            if (!componentInterface || !componentInterface->IsEntityTag() || !componentInterface->GetShouldSerialize())
            {
                continue;
            }

            const EntityTag tag = componentInterface->GetEntityTag();

            if (ShouldSkipEntityTagForSerialization(tag))
            {
                continue;
            }

            resultTags.PushBack(tag);
        }
    };

    if (IsOnThread(entityManager->GetOwnerThreadId()))
    {
        SerializeEntityTags();
    }
    else
    {
        HYP_NAMED_SCOPE("Awaiting async entity tag serialization");

        Task<void> task = GetThreadById(entityManager->GetOwnerThreadId())->GetScheduler().Enqueue(HYP_STATIC_MESSAGE("Serialize Entity Tags"), [&SerializeEntityTags]()
                                                                                                   {
                                                                                                       SerializeEntityTags();
                                                                                                   });

        task.Await();
    }

    return resultTags;
}

void Entity::DeserializeTags(const Array<EntityTag>& tags)
{
    AssertDebug(m_scene != nullptr);

    if (!m_entityManager)
    {
        SetEntityManager(m_scene->GetEntityManager());
    }

    AssertDebug(m_entityManager != nullptr);

    for (const EntityTag& tag : tags)
    {
        if (ShouldSkipEntityTagForSerialization(tag))
        {
            continue;
        }

        m_entityManager->AddTag(this, tag);
    }
}

Array<BoxedValue, DynamicAllocator> Entity::SerializeComponents() const
{
    EntityManager* entityManager = GetEntityManager();

    if (!entityManager)
    {
        return {};
    }

    Array<BoxedValue, DynamicAllocator> resultArray;

    auto SerializeEntityAndComponents = [this, entityManager, &resultArray]()
    {
        Optional<const ComponentMap&> allComponentsOpt = entityManager->GetAllComponents(this);

        if (!allComponentsOpt.HasValue())
        {
            HYP_LOG(Serialization, Error, "No component map found for entity");

            return;
        }

        Set<TypeId> serializedComponents;

        for (const auto& it : *allComponentsOpt)
        {
            const TypeId componentTypeId = it.first;

            const IComponentInterface* componentInterface = ComponentInterfaceRegistry::GetInstance().GetComponentInterface(componentTypeId);

            if (!componentInterface)
            {
                HYP_LOG(Serialization, Error, "No ComponentInterface registered for component with TypeId {}", componentTypeId.Value());

                return;
            }

            if (!componentInterface->GetShouldSerialize())
            {
                continue;
            }

            if (serializedComponents.Contains(componentTypeId))
            {
                HYP_LOG(Serialization, Warning, "Entity has multiple components of the type {}", componentInterface->GetTypeInfo().name);

                continue;
            }

            if (componentInterface->IsEntityTag())
            {
                // tags are serialized separately via the "Tags" property
                continue;
            }

            // Create a copy of the component with transient fields stripped,
            // so cloning doesn't copy unintended runtime state
            BoxedValue componentData;
            CloneWithoutTransientMembers(
                BoxedValue(entityManager->TryGetComponent(componentTypeId, this)),
                componentData);

            resultArray.PushBack(std::move(componentData));
            serializedComponents.Insert(componentTypeId);
        }
    };

    if (IsOnThread(entityManager->GetOwnerThreadId()))
    {
        SerializeEntityAndComponents();
    }
    else
    {
        HYP_NAMED_SCOPE("Awaiting async entity and component serialization");

        Task<void> serializeEntityAndComponentsTask = GetThreadById(entityManager->GetOwnerThreadId())->GetScheduler().Enqueue(HYP_STATIC_MESSAGE("Serialize Entity and Components"), [&SerializeEntityAndComponents]()
                                                                                                                               {
                                                                                                                                   SerializeEntityAndComponents();
                                                                                                                               });

        serializeEntityAndComponentsTask.Await();
    }

    return resultArray;
}

void Entity::DeserializeComponents(const Array<BoxedValue, DynamicAllocator>& components)
{
    AssertDebug(m_scene != nullptr);

    if (!m_entityManager)
    {
        SetEntityManager(m_scene->GetEntityManager());
    }

    AssertDebug(m_entityManager != nullptr);

    for (const BoxedValue& componentData : components)
    {
        const TypeInfo& componentTypeInfo = *componentData.GetTypeInfo();

        if (!m_entityManager->IsValidComponentType(componentTypeInfo.id))
        {
            HYP_LOG(Serialization, Warning, "{} is not a valid component type", componentTypeInfo.name);

            continue;
        }

        const IComponentInterface* componentInterface = ComponentInterfaceRegistry::GetInstance().GetComponentInterface(componentTypeInfo.id);

        if (!componentInterface)
        {
            HYP_LOG(Serialization, Warning, "No ComponentInterface registered for {}", componentTypeInfo.name);

            continue;
        }

        if (!componentInterface->GetShouldSerialize())
        {
            HYP_LOG(Serialization, Warning, "Component of type {} is not marked for serialization", componentTypeInfo.name);

            continue;
        }

        if (componentInterface->IsEntityTag())
        {
            // tags are serialized/deserialized separately via the "Tags" property
            continue;
        }

        HYP_NAMED_SCOPE_FMT("Deserializing component '{}'", componentTypeInfo.name);

        if (m_entityManager->HasComponent(componentTypeInfo.id, this))
        {
            HYP_LOG(Serialization, Warning, "Entity already has component '{}'", componentTypeInfo.name);

            continue;
        }

        HYP_LOG(Serialization, Verbose, "Adding component '{}' to entity of type {} with id: {}",
                componentTypeInfo.name,
                InstanceClass()->GetName(),
                Id());

        m_entityManager->AddComponent(this, componentData);
    }
}

#pragma endregion Entity

} // namespace Hyperion
