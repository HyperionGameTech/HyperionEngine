/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <scene/Entity.hpp>

#include <scene/ecs/EntityManager.hpp>
#include <scene/ecs/ComponentInterface.hpp>

#include <core/logging/Logger.hpp>
#include <core/logging/LogChannels.hpp>

#include <core/object/HypClass.hpp>

#include <core/profiling/ProfileScope.hpp>

namespace hyperion {

Entity::Entity() = default;

Entity::~Entity()
{
    const ID<Entity> id = GetID();

    if (!id.IsValid()) {
        return;
    }

    // Keep a WeakHandle of Entity so the ID doesn't get reused while we're using it
    EntityManager::GetEntityToEntityManagerMap().PerformActionWithEntity_FireAndForget(id, [weak_this = WeakHandleFromThis()](EntityManager *entity_manager, ID<Entity> id)
    {
        HYP_NAMED_SCOPE("Remove Entity from EntityManager (task)");

        HYP_LOG(ECS, Debug, "Removing entity #{} from entity manager", id.Value());

        if (entity_manager->HasEntity(id)) {

            for (const Pair<TypeID, ComponentID> &it : *entity_manager->GetAllComponents(id)) {
                AnyRef component_ref = entity_manager->TryGetComponent(it.first, id);

                if (component_ref.HasValue()) {
                    const IComponentInterface *component_interface = ComponentInterfaceRegistry::GetInstance().GetComponentInterface(it.first);
                    AssertThrow(component_interface != nullptr);

                    if (component_interface->IsEntityTag()) {
                        HYP_LOG(ECS, Debug, "  Removing entity tag component of type '{}' from Entity #{}", component_interface->GetTypeName(), id.Value());
                    } else {
                        HYP_LOG(ECS, Debug, "  Removing component of type '{}' from Entity #{}", component_interface->GetTypeName(), id.Value());
                    }
                }
            }

            entity_manager->RemoveEntity(id);
        }
    });
}

} // namespace hyperion