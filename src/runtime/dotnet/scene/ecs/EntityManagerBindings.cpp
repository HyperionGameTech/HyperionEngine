#include <scene/ecs/EntityManager.hpp>

// Components
#include <scene/ecs/components/TransformComponent.hpp>

#include <runtime/dotnet/scene/ManagedSceneTypes.hpp>

#include <Engine.hpp>
#include <Types.hpp>

using namespace hyperion;
using namespace hyperion::v2;

extern "C" {
    ManagedEntity EntityManager_AddEntity(EntityManager *manager)
    {
        return manager->AddEntity();
    }

    void EntityManager_RemoveEntity(EntityManager *manager, ManagedEntity entity)
    {
        manager->RemoveEntity(entity);
    }

    bool EntityManager_HasEntity(EntityManager *manager, ManagedEntity entity)
    {
        return manager->HasEntity(entity);
    }

    bool EntityManager_HasComponent(EntityManager *manager, UInt32 component_type_id, ManagedEntity entity)
    {
        return manager->HasComponent(TypeID { component_type_id }, entity);
    }

    void *EntityManager_GetComponent(EntityManager *manager, UInt32 component_type_id, ManagedEntity entity)
    {
        return manager->TryGetComponent(TypeID { component_type_id }, entity);
    }

    // Components
    UInt32 TransformComponent_GetNativeTypeID()
    {
        return TypeID::ForType<TransformComponent>().Value();
    }

    ComponentID TransformComponent_AddComponent(EntityManager *manager, ManagedEntity entity, TransformComponent *component)
    {
        return manager->AddComponent(entity, std::move(*component));
    }
}