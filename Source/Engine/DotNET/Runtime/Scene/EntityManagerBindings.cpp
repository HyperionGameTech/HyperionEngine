/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#include <HyperionPch.hpp>

#include <Scene/EntityManager.hpp>

// Components
#include <Scene/Components/TransformComponent.hpp>
#include <Scene/Components/MeshComponent.hpp>
#include <Scene/Components/BoundingBoxComponent.hpp>
#include <Scene/Components/VisibilityStateComponent.hpp>
#include <Scene/Components/UIComponent.hpp>

#include <Scene/Animation/Skeleton.hpp>

using namespace Hyperion;

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

        BoxedValue boxed(AnyRef(&typeInfo, pComponent));
        pManager->AddComponent(pEntity, boxed);
    }

    HYP_EXPORT int8 EntityManager_AddTypedEntity(EntityManager* pManager, const Class* pClass, BoxedValue* pOutBoxed)
    {
        Assert(pManager != nullptr);
        Assert(pClass != nullptr);
        Assert(pOutBoxed != nullptr);

        Handle<Entity> entityHandle = pManager->AddTypedEntity(pClass);

        if (!entityHandle.IsValid())
        {
            return false;
        }

        *pOutBoxed = BoxedValue(entityHandle);

        return true;
    }

    HYP_EXPORT void EntityManager_AddTag(EntityManager* pManager, Entity* pEntity, uint64 tag)
    {
        if (!pManager || !pEntity)
        {
            return;
        }

        pManager->AddTag(pEntity, EntityTag(tag));
    }

    HYP_EXPORT int8 EntityManager_RemoveTag(EntityManager* pManager, Entity* pEntity, uint64 tag)
    {
        if (!pManager || !pEntity)
        {
            return false;
        }

        return pManager->RemoveTag(pEntity, EntityTag(tag));
    }

    HYP_EXPORT int8 EntityManager_HasTag(EntityManager* pManager, Entity* pEntity, uint64 tag)
    {
        if (!pManager || !pEntity)
        {
            return false;
        }

        return pManager->HasTag(pEntity, EntityTag(tag));
    }

} // extern "C"
