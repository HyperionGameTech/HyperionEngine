/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <ScenePch.hpp>

#include <scene/Entity.hpp>
#include <scene/Scene.hpp>
#include <scene/World.hpp>
#include <scene/Node.hpp>
#include <scene/DetachedScene.hpp>

#include <scene/util/EntityScripting.hpp>

#include <scene/EntityManager.hpp>
#include <scene/EntityTag.hpp>

#include <scene/ComponentInterface.hpp>
#include <scene/components/MeshComponent.hpp>
#include <scene/components/ScriptComponent.hpp>
#include <scene/components/TransformComponent.hpp>
#include <scene/components/VisibilityStateComponent.hpp>
#include <scene/components/BoundingBoxComponent.hpp>

#include <rendering/Mesh.hpp>
#include <rendering/Material.hpp>
#include <rendering/RenderProxy.hpp>

#include <engine/EngineDriver.hpp>

#include <Entity.generated.inl>

namespace Hyperion {

Entity::Entity()
    : Entity(Name::Invalid())
{
}

Entity::Entity(Name name)
    : Node(name),
      m_world(nullptr),
      m_entityManager(nullptr),
      m_renderProxyVersion(0),
      m_transformChanged(false)
{
}

Entity::~Entity()
{
    m_scene = nullptr;
    m_world = nullptr;

    EntityManager* entityManager = GetEntityManager();
    if (entityManager == nullptr)
    {
        return;
    }

    if (IsOnThread(entityManager->GetOwnerThreadId()))
    {
        HYP_NAMED_SCOPE("Remove Entity from EntityManager (sync)");

        HYP_LOG(Entity, Verbose, "Removing Entity {} from entity manager", Id());

        if (!entityManager->RemoveEntity(this))
        {
            HYP_LOG(Entity, Error, "Failed to remove Entity {} from EntityManager", Id());
        }
    }
    else
    {
        // If not on the correct thread, perform the removal asynchronously
        // Keep a WeakHandle of Entity so the Id doesn't get reused while we're using it
        GetThreadById(entityManager->GetOwnerThreadId())->GetScheduler().Enqueue([weakThis = MakeWeakRef(this), entityManagerWeak = MakeWeakRef(entityManager)]()
            {
                Handle<EntityManager> entityManager = entityManagerWeak.Lock();
                if (!entityManager)
                {
                    HYP_LOG(Entity, Error, "EntityManager is no longer valid while removing Entity {}", weakThis.Id());
                    return;
                }

                HYP_NAMED_SCOPE("Remove Entity from EntityManager (async)");

                HYP_LOG(Entity, Verbose, "Removing Entity {} from entity manager", weakThis.Id());

                if (!entityManager->RemoveEntity(weakThis.GetUnsafe()))
                {
                    HYP_LOG(Entity, Error, "Failed to remove Entity {} from EntityManager", weakThis.Id());
                }
            },
            TaskEnqueueFlags::FIRE_AND_FORGET);
    }
}

void Entity::Init()
{
    HYP_SCOPE;

    AssertDebug(m_scene != nullptr);
    SetEntityManager(m_scene->GetEntityManager());

    Node::Init();

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
    AssertDebug(world != nullptr);

    m_world = world;
}

void Entity::OnRemovedFromWorld(World* world)
{
    AssertDebug(world != nullptr);
    AssertDebug(m_world == world);

    m_world = nullptr;
}

void Entity::OnAddedToScene(Scene* scene)
{
    AssertDebug(scene != nullptr);

    EntityManager* entityManager = nullptr;
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
    HYP_SCOPE;

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

        if (m_entityInitInfo.bvhDepth > 0)
        {
            if (meshComponent->mesh->GetBVH().IsValid())
            {
                // already has a BVH, skip
                return;
            }

            if (!meshComponent->mesh->BuildBVH(m_entityInitInfo.bvhDepth))
            {
                HYP_LOG(Entity, Error, "Failed to build BVH for MeshComponent on Entity {}!", Id());

                return;
            }
        }

        AddTag<EntityTag::UpdateRenderProxy>();

        return;
    }
}

void Entity::OnComponentRemoved(AnyRef component)
{
}

void Entity::OnTagAdded(EntityTag tag)
{
}

void Entity::OnTagRemoved(EntityTag tag)
{
}

void Entity::SetScene(Scene* scene)
{
    if (scene == m_scene)
    {
        return;
    }

    Node::SetScene(scene);

    // Move entity from previous scene to new scene's EntityManager
    SetEntityManager(m_scene->GetEntityManager());
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
    }
}

void Entity::OnTransformUpdated()
{
    HYP_SCOPE;

    Node::OnTransformUpdated();

    if (!IsInitCalled())
    {
        return;
    }

    EntityManager* entityManager = GetEntityManager();
    AssertDebug(entityManager != nullptr);
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

    entityManager->AddTags<EntityTag::UpdateVisibility, EntityTag::UpdateRenderProxy>(this);
}

void Entity::OnMobilityChanged(bool isStatic)
{
    HYP_SCOPE;

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
    HYP_SCOPE;

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

Array<BoxedValue, DynamicAllocator> Entity::SerializeComponents() const
{
    HYP_SCOPE;

    EntityManager* entityManager = GetEntityManager();

    if (!entityManager)
    {
        return {};
    }

    Array<BoxedValue, DynamicAllocator> resultArray;

    auto SerializeEntityAndComponents = [this, entityManager, &resultArray]()
    {
        Optional<const TypeMap<ComponentId>&> allComponents = entityManager->GetAllComponents(this);

        if (!allComponents.HasValue())
        {
            HYP_LOG(Serialization, Error, "No component map found for entity");

            return;
        }

        HashSet<TypeId> serializedComponents;

        for (const auto& it : *allComponents)
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
                // tags are serialized separately
                continue;
            }

            resultArray.PushBack(BoxedValue(entityManager->TryGetComponent(componentTypeId, this)));
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
    HYP_SCOPE;

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
            continue; // tags are deserialized separately

            HYP_NAMED_SCOPE("Deserializing entity tag");

            Optional<EntityTag> entityTagOpt = componentData.TryGet<EntityTag>();

            if (!entityTagOpt.HasValue())
            {
                HYP_LOG(Serialization, Error, "Failed to deserialize entity tag component of type {}", componentTypeInfo.name);

                continue;
            }

            const TypeId entityTagTypeId = componentInterface->GetTypeInfo().id;

            if (!m_entityManager->IsEntityTagComponent(entityTagTypeId))
            {
                HYP_LOG(Serialization, Warning, "Component {} is not an entity tag component", componentInterface->GetTypeInfo().name);

                continue;
            }

            if (entityTagTypeId == TypeId::ForType<TagComponent<EntityTag::MobStatic>>()
                || entityTagTypeId == TypeId::ForType<TagComponent<EntityTag::MobDynamic>>())
            {
                // we now handle these tags based on the Entity's mobility, skip
                continue;
            }

            m_entityManager->AddTag(this, *entityTagOpt);

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

} // namespace Hyperion
