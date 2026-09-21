/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#include <HyperionPch.hpp>

#include <Scene/EntityManager.hpp>
#include <Scene/ComponentInterface.hpp>
#include <Scene/Entity.hpp>
#include <Scene/World.hpp>
#include <Scene/Systems/SwatchOverrideSystem.hpp>

// Components
#include <Scene/Components/TransformComponent.hpp>
#include <Scene/Components/MeshComponent.hpp>
#include <Scene/Components/BoundingBoxComponent.hpp>
#include <Scene/Components/VisibilityStateComponent.hpp>
#include <Scene/Components/UIComponent.hpp>

#include <Scene/Animation/Skeleton.hpp>

#include <Core/Reflection/Struct.hpp>

#include <Core/Logging/Logger.hpp>
#include <Core/Logging/LogChannels.hpp>

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

    /// Adds component, if pComponent is null it will create a new instance.
    /// Returns false without adding anything when the entity already has the component.
    HYP_EXPORT int8 EntityManager_AddComponent(EntityManager* pManager, Entity* pEntity, uint32 componentTypeIdValue, void* pComponent)
    {
        Assert(pManager != nullptr);
        Assert(pEntity != nullptr);

        const TypeId componentTypeId { componentTypeIdValue };

        const ComponentInterface* pComponentInterface = ComponentInterfaceRegistry::GetInstance().GetComponentInterface(componentTypeId);
        Assert(pComponentInterface != nullptr, "Invalid component type!");

        if (!pComponentInterface)
        {
            return false;
        }

        if (!pComponentInterface->IsEntityTag() && pManager->HasComponent(componentTypeId, pEntity))
        {
            HYP_LOG(Scene, Warning, "Entity #{} already has a '{}' component; keeping the existing one. Check HasComponent before adding.",
                pEntity->Id(), *EntityManager::GetComponentTypeName(componentTypeId));

            return false;
        }

        if (pComponent != nullptr)
        {
            pManager->AddComponent(pEntity, BoxedValue(AnyRef(&pComponentInterface->GetTypeInfo(), pComponent)));
        }
        else
        {
            // Make instance if pComponent is null
            pManager->AddDefaultComponent(pEntity, componentTypeId);
        }

        return true;
    }

    HYP_EXPORT int8 EntityManager_RemoveComponent(EntityManager* pManager, uint32 componentTypeIdValue, Entity* pEntity)
    {
        Assert(pManager != nullptr);
        Assert(pEntity != nullptr);

        const TypeId componentTypeId { componentTypeIdValue };

        return pManager->RemoveComponent(componentTypeId, pEntity);
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

    HYP_EXPORT uint32 EntityTag_GetEditorFriendlyTags(uint64* pOutTags)
    {
        const Array<const ComponentInterface*> componentInterfaces = ComponentInterfaceRegistry::GetInstance().GetComponentInterfaces();

        uint32 numTags = 0;

        for (const ComponentInterface* componentInterface : componentInterfaces)
        {
            if (!componentInterface->IsEntityTag() || !componentInterface->ShouldShowInEditor())
            {
                continue;
            }

            if (pOutTags != nullptr)
            {
                pOutTags[numTags] = uint64(componentInterface->GetEntityTag());
            }

            ++numTags;
        }

        return numTags;
    }

    HYP_EXPORT int8 ComponentInterfaceRegistry_RegisterRuntimeComponent(const Class* pClass, uint32 flags)
    {
        const Struct* pStruct = GetStructFromClass(pClass);

        if (!pStruct)
        {
            return false;
        }

        return ComponentInterfaceRegistry::GetInstance().RegisterRuntimeComponent(pStruct, EnumFlags<ComponentInterfaceFlags>(static_cast<ComponentInterfaceFlags>(flags))) != nullptr;
    }

    HYP_EXPORT int8 ComponentInterfaceRegistry_UnregisterRuntimeComponent(uint32 componentTypeIdValue)
    {
        return ComponentInterfaceRegistry::GetInstance().UnregisterRuntimeComponent(TypeId { componentTypeIdValue });
    }


    static SwatchOverrideSystem* GetSwatchOverrideSystemForEntity(const Entity* pEntity)
    {
        if (!pEntity)
        {
            return nullptr;
        }

        World* world = pEntity->GetWorld();

        if (!world)
        {
            return nullptr;
        }

        return world->GetSystem<SwatchOverrideSystem>();
    }

    HYP_EXPORT int8 EntitySwatchOverrides_SetValue(Entity* pEntity, uint64 swatchHash, uint64 propertyHash, BoxedValue* pValue)
    {
        SwatchOverrideSystem* system = GetSwatchOverrideSystemForEntity(pEntity);

        if (!pValue || !system)
        {
            return false;
        }

        return system->SetSwatchOverrideValue(pEntity, Name(NameID(swatchHash)), Name(NameID(propertyHash)), *pValue);
    }

    HYP_EXPORT int8 EntitySwatchOverrides_RemoveValue(Entity* pEntity, uint64 swatchHash, uint64 propertyHash)
    {
        SwatchOverrideSystem* system = GetSwatchOverrideSystemForEntity(pEntity);

        if (!system)
        {
            return false;
        }

        return system->RemoveSwatchOverrideValue(pEntity, Name(NameID(swatchHash)), Name(NameID(propertyHash)));
    }

    HYP_EXPORT int8 EntitySwatchOverrides_GetBaseValue(Entity* pEntity, uint64 swatchHash, uint64 propertyHash, BoxedValue* pOutValue)
    {
        SwatchOverrideSystem* system = GetSwatchOverrideSystemForEntity(pEntity);

        if (!pOutValue || !system)
        {
            return false;
        }

        return system->GetSwatchOverrideBaseValue(pEntity, Name(NameID(swatchHash)), Name(NameID(propertyHash)), *pOutValue);
    }

    HYP_EXPORT int8 EntitySwatchOverrides_GetValue(Entity* pEntity, uint64 swatchHash, uint64 propertyHash, BoxedValue* pOutValue)
    {
        SwatchOverrideSystem* system = GetSwatchOverrideSystemForEntity(pEntity);

        if (!pOutValue || !system)
        {
            return false;
        }

        return system->GetSwatchOverrideValue(pEntity, Name(NameID(swatchHash)), Name(NameID(propertyHash)), *pOutValue);
    }

    HYP_EXPORT int8 EntitySwatchOverrides_SetBaseValue(Entity* pEntity, uint64 propertyHash, BoxedValue* pValue)
    {
        SwatchOverrideSystem* system = GetSwatchOverrideSystemForEntity(pEntity);

        if (!pValue || !system)
        {
            return false;
        }

        return system->SetSwatchOverrideBaseValue(pEntity, Name(NameID(propertyHash)), *pValue);
    }
} // extern "C"