/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <scene/EntityManager.hpp>

// Components
#include <scene/components/TransformComponent.hpp>
#include <scene/components/MeshComponent.hpp>
#include <scene/components/BoundingBoxComponent.hpp>
#include <scene/components/VisibilityStateComponent.hpp>
#include <scene/components/UIComponent.hpp>

#include <scene/animation/Skeleton.hpp>

#include <core/logging/LogChannels.hpp>
#include <core/logging/Logger.hpp>

#include <core/Types.hpp>

using namespace hyperion;

extern "C"
{

    HYP_EXPORT bool EntityManager_HasComponent(EntityManager* manager, uint32 componentTypeIdValue, Entity* entity)
    {
        Assert(manager != nullptr);
        Assert(entity != nullptr);

        const TypeId componentTypeId { componentTypeIdValue };

        return manager->HasComponent(componentTypeId, entity);
    }

    HYP_EXPORT void* EntityManager_GetComponent(EntityManager* manager, uint32 componentTypeIdValue, Entity* entity)
    {
        Assert(manager != nullptr);
        Assert(entity != nullptr);

        const TypeId componentTypeId { componentTypeIdValue };

        return manager->TryGetComponent(componentTypeId, entity).GetPointer();
    }

    HYP_EXPORT void EntityManager_AddComponent(EntityManager* manager, Entity* entity, uint32 componentTypeIdValue, HypData* componentHypData)
    {
        Assert(manager != nullptr);
        Assert(entity != nullptr);
        Assert(componentHypData != nullptr);

        const TypeId componentTypeId { componentTypeIdValue };

        Assert(manager->IsValidComponentType(componentTypeId));

        Handle<Entity> entityHandle = MakeStrongRef(entity);

        manager->AddComponent(entityHandle, *componentHypData);
    }

    HYP_EXPORT int8 EntityManager_AddTypedEntity(EntityManager* manager, const HypClass* hypClass, HypData* outHypData)
    {
        Assert(manager != nullptr);
        Assert(hypClass != nullptr);
        Assert(outHypData != nullptr);

        Handle<Entity> entityHandle = manager->AddTypedEntity(hypClass);

        if (!entityHandle.IsValid())
        {
            return false;
        }

        *outHypData = HypData(entityHandle);

        return true;
    }

} // extern "C"
