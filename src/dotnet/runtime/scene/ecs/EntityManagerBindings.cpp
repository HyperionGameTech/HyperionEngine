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

    HYP_EXPORT bool EntityManager_HasComponent(EntityManager* manager, uint32 component_type_id, Entity* entity)
    {
        AssertThrow(manager != nullptr);
        AssertThrow(entity != nullptr);

        return manager->HasComponent(TypeID { component_type_id }, entity);
    }

    HYP_EXPORT void* EntityManager_GetComponent(EntityManager* manager, uint32 component_type_id, Entity* entity)
    {
        AssertThrow(manager != nullptr);
        AssertThrow(entity != nullptr);

        return manager->TryGetComponent(TypeID { component_type_id }, entity).GetPointer();
    }

    HYP_EXPORT void EntityManager_AddComponent(EntityManager* manager, Entity* entity, uint32 component_type_id, HypData* component_hyp_data)
    {
        AssertThrow(manager != nullptr);
        AssertThrow(entity != nullptr);
        AssertThrow(component_hyp_data != nullptr);

        AssertThrow(manager->IsValidComponentType(TypeID { component_type_id }));

        Handle<Entity> entity_handle = entity->HandleFromThis();
        AssertThrow(entity_handle.IsValid());

        manager->AddComponent(entity_handle, *component_hyp_data);
    }

} // extern "C"
