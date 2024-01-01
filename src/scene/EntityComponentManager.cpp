#include <scene/EntityComponentManager.hpp>

namespace hyperion::v2 {

namespace detail {

Controller *ComponentContainerBase::Get(ID<Entity> id)
{
    auto it = Find(id);

    if (it == End()) {
        return nullptr;
    }

    return it->second.Get();
}

Controller *ComponentContainerBase::Add(Entity *entity, UniquePtr<Controller> &&controller)
{
    AssertThrow(entity != nullptr);
    AssertThrow(controller != nullptr);
    AssertThrow(controller->GetOwner() == nullptr);

    controller->SetOwner(entity);
    controller->OnAdded();

    for (Node *node : entity->GetNodes()) {
        controller->OnAttachedToNode(node);
    }

    for (const ID<Scene> &id : entity->GetScenes()) {
        controller->OnAttachedToScene(id);
    }

    controller->OnTransformUpdate(entity->GetTransform());

    auto it = ComponentContainerBase::m_entity_to_controller.Find(entity->GetID());

    if (it != ComponentContainerBase::m_entity_to_controller.End()) {
        Controller *found_controller = it->second.Get();
        AssertThrow(found_controller != nullptr);

        if (found_controller->GetOwner() != nullptr) {
            for (Node *node : found_controller->GetOwner()->GetNodes()) {
                controller->OnDetachedFromNode(node);
            }

            for (ID<Scene> id : found_controller->GetOwner()->GetScenes()) {
                controller->OnDetachedFromScene(id);
            }
        }

        found_controller->OnRemoved();

        // Controller already exists, replace it.
        it->second = std::move(controller);

        return it->second.Get();
    } else {
        auto insert_result = ComponentContainerBase::m_entity_to_controller.Set(entity->GetID(), std::move(controller));
        auto iter = insert_result.first;

        return iter->second.Get();
    }
}

Bool ComponentContainerBase::Remove(ID<Entity> id)
{
    auto it = ComponentContainerBase::m_entity_to_controller.Find(id);

    if (it == ComponentContainerBase::m_entity_to_controller.End()) {
        return false;
    }

    Controller *controller = it->second.Get();
    AssertThrow(controller->GetOwner() != nullptr);

    for (Node *node : controller->GetOwner()->GetNodes()) {
        controller->OnDetachedFromNode(node);
    }

    for (ID<Scene> id : controller->GetOwner()->GetScenes()) {
        controller->OnDetachedFromScene(id);
    }

    controller->OnRemoved();

    return true;
}

} // namespace detail

} // namespace hyperion::v2