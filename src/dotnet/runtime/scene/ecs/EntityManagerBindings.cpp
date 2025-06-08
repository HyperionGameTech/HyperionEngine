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

#include <core/logging/LogChannels.hpp>
#include <core/logging/Logger.hpp>

#include <Types.hpp>

using namespace hyperion;

extern "C"
{

    HYP_EXPORT bool EntityManager_HasComponent(EntityManager* manager, uint32 component_type_id, IDBase::ValueType entity_id)
    {
        AssertThrow(manager != nullptr);

        return manager->HasComponent(TypeID { component_type_id }, ID<Entity> { entity_id });
    }

    HYP_EXPORT void* EntityManager_GetComponent(EntityManager* manager, uint32 component_type_id, IDBase::ValueType entity_id)
    {
        AssertThrow(manager != nullptr);

        return manager->TryGetComponent(TypeID { component_type_id }, ID<Entity> { entity_id }).GetPointer();
    }

    HYP_EXPORT void EntityManager_AddComponent(EntityManager* manager, IDBase::ValueType entity_id, uint32 component_type_id, void* component_ptr)
    {
        AssertThrow(manager != nullptr);
        AssertThrow(component_ptr != nullptr);

        AssertThrow(manager->IsValidComponentType(TypeID { component_type_id }));

        manager->AddComponent(ID<Entity> { entity_id }, AnyRef(TypeID { component_type_id }, component_ptr));
    }

} // extern "C"
