/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <scene/ecs/EntityManager.hpp>

// Components
#include <scene/ecs/components/TransformComponent.hpp>
#include <scene/ecs/components/MeshComponent.hpp>
#include <scene/ecs/components/BoundingBoxComponent.hpp>
#include <scene/ecs/components/VisibilityStateComponent.hpp>
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

        return manager->HasComponent(TypeId { component_type_id }, entity);
    }

    HYP_EXPORT void* EntityManager_GetComponent(EntityManager* manager, uint32 component_type_id, Entity* entity)
    {
        AssertThrow(manager != nullptr);
        AssertThrow(entity != nullptr);

        return manager->TryGetComponent(TypeId { component_type_id }, entity).GetPointer();
    }

    HYP_EXPORT void EntityManager_AddComponent(EntityManager* manager, Entity* entity, uint32 component_type_id, HypData* component_hyp_data)
    {
        AssertThrow(manager != nullptr);
        AssertThrow(entity != nullptr);
        AssertThrow(component_hyp_data != nullptr);

        AssertThrow(manager->IsValidComponentType(TypeId { component_type_id }));

        Handle<Entity> entity_handle = entity->HandleFromThis();
        AssertThrow(entity_handle.IsValid());

        manager->AddComponent(entity_handle, *component_hyp_data);
    }

    HYP_EXPORT int8 EntityManager_AddTypedEntity(EntityManager* manager, const HypClass* hyp_class, HypData* out_hyp_data)
    {
        AssertThrow(manager != nullptr);
        AssertThrow(hyp_class != nullptr);
        AssertThrow(out_hyp_data != nullptr);

        Handle<Entity> entity_handle = manager->AddTypedEntity(hyp_class);

        if (!entity_handle.IsValid())
        {
            return false;
        }

        *out_hyp_data = HypData(entity_handle);

        return true;
    }

} // extern "C"
