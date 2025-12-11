/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <HyperionPch.hpp>

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

using namespace hyperion;

extern "C"
{

    HYP_EXPORT bool EntityManager_HasComponent(EntityManager* pManager, uint32 componentTypeIdValue, Entity* pEntity)
    {
        Assert(pManager != nullptr);
        Assert(pEntity != nullptr);

        const TypeId componentTypeId { componentTypeIdValue };

        return pManager->HasComponent(componentTypeId, pEntity);
    }

    HYP_EXPORT void* EntityManager_GetComponent(EntityManager* pManager, uint32 componentTypeIdValue, Entity* pEntity)
    {
        Assert(pManager != nullptr);
        Assert(pEntity != nullptr);

        const TypeId componentTypeId { componentTypeIdValue };

        return pManager->TryGetComponent(componentTypeId, pEntity).GetPointer();
    }

    HYP_EXPORT uint32 EntityManager_GetComponents(EntityManager* pManager, Entity* pEntity, const ComponentMap** ppOutComponents)
    {
        Assert(pManager != nullptr);
        Assert(pEntity != nullptr);
        Assert(ppOutComponents != nullptr);

        Optional<const ComponentMap&> componentsOpt = pManager->GetAllComponents(pEntity);

        if (!componentsOpt)
        {
            *ppOutComponents = nullptr;

            return 0;
        }

        *ppOutComponents = &*componentsOpt;

        return uint32(componentsOpt->Size());
    }

    HYP_EXPORT uint32 EntityManager_GetComponentTypeIds(EntityManager* pManager, Entity* pEntity, uint32* pOutComponentTypeIds)
    {
        Assert(pManager != nullptr);
        Assert(pEntity != nullptr);

        // pOutComponentTypeIds can be nullptr, called twice to get size first

        Optional<const ComponentMap&> componentsOpt = pManager->GetAllComponents(pEntity);

        if (!componentsOpt)
        {
            return 0;
        }

        const ComponentMap& components = *componentsOpt;

        if (pOutComponentTypeIds == nullptr)
        {
            return uint32(componentsOpt->Size());
        }

        // Fill the output array (assumed to have correct size)
        uint32 i = 0;
        for (const KeyValuePair<TypeId, ComponentId>& it : components)
        {
            pOutComponentTypeIds[i++] = it.first.Value();
        }

        return i;
    }

    HYP_EXPORT void EntityManager_AddComponent(EntityManager* pManager, Entity* pEntity, uint32 componentTypeIdValue, void* pComponent)
    {
        Assert(pManager != nullptr);
        Assert(pEntity != nullptr);
        Assert(pComponent != nullptr);

        const TypeId componentTypeId { componentTypeIdValue };

        ComponentContainerBase* pContainer = pManager->TryGetContainer(componentTypeId);
        Assert(pContainer != nullptr, "Invalid component type!");

        const TypeInfo& typeInfo = pContainer->GetComponentTypeInfo();

        HypData hd(AnyRef(&typeInfo, pComponent));
        pManager->AddComponent(pEntity, hd);
    }

    HYP_EXPORT int8 EntityManager_AddTypedEntity(EntityManager* pManager, const Class* pClass, HypData* pOutHypData)
    {
        Assert(pManager != nullptr);
        Assert(pClass != nullptr);
        Assert(pOutHypData != nullptr);

        Handle<Entity> entityHandle = pManager->AddTypedEntity(pClass);

        if (!entityHandle.IsValid())
        {
            return false;
        }

        *pOutHypData = HypData(entityHandle);

        return true;
    }

} // extern "C"
