/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <scene/ecs/EntityManager.hpp>

// Components
#include <scene/ecs/components/TransformComponent.hpp>
#include <scene/ecs/components/MeshComponent.hpp>
#include <scene/ecs/components/BoundingBoxComponent.hpp>
#include <scene/ecs/components/VisibilityStateComponent.hpp>
#include <scene/ecs/components/LightComponent.hpp>
#include <scene/ecs/components/ShadowMapComponent.hpp>
#include <scene/ecs/components/UIComponent.hpp>
#include <scene/ecs/components/NodeLinkComponent.hpp>

#include <scene/animation/Skeleton.hpp>

#include <dotnet/runtime/scene/ManagedSceneTypes.hpp>

#include <core/logging/LogChannels.hpp>
#include <core/logging/Logger.hpp>

#include <Types.hpp>

using namespace hyperion;

extern "C" {
HYP_EXPORT ManagedEntity EntityManager_AddEntity(EntityManager *manager)
{
    AssertThrow(manager != nullptr);

    return manager->AddEntity();
}

HYP_EXPORT void EntityManager_RemoveEntity(EntityManager *manager, ManagedEntity entity)
{
    AssertThrow(manager != nullptr);
    
    manager->RemoveEntity(entity);
}

HYP_EXPORT bool EntityManager_HasEntity(EntityManager *manager, ManagedEntity entity)
{
    AssertThrow(manager != nullptr);
    
    return manager->HasEntity(entity);
}

HYP_EXPORT bool EntityManager_HasComponent(EntityManager *manager, uint32 component_type_id, ManagedEntity entity)
{
    AssertThrow(manager != nullptr);
    
    return manager->HasComponent(TypeID { component_type_id }, entity);
}

HYP_EXPORT void *EntityManager_GetComponent(EntityManager *manager, uint32 component_type_id, ManagedEntity entity)
{
    AssertThrow(manager != nullptr);
    
    return manager->TryGetComponent(TypeID { component_type_id }, entity).GetPointer();
}

HYP_EXPORT ComponentID EntityManager_AddComponent(EntityManager *manager, ManagedEntity entity, uint32 component_type_id, void *component_ptr)
{
    AssertThrow(manager != nullptr);
    AssertThrow(component_ptr != nullptr);

    if (!manager->IsValidComponentType(TypeID { component_type_id })) {
        return EntityManager::invalid_component_id;
    }

    return manager->AddComponent(entity, AnyRef(TypeID { component_type_id }, component_ptr));
}

} // extern "C"
