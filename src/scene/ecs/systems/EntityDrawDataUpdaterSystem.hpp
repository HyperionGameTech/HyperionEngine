#ifndef HYPERION_V2_ECS_ENTITY_DRAW_DATA_UPDATER_SYSTEM_HPP
#define HYPERION_V2_ECS_ENTITY_DRAW_DATA_UPDATER_SYSTEM_HPP

#include <scene/ecs/System.hpp>
#include <scene/ecs/components/MeshComponent.hpp>
#include <scene/ecs/components/TransformComponent.hpp>
#include <scene/ecs/components/BoundingBoxComponent.hpp>
#include <rendering/EntityDrawData.hpp>

namespace hyperion::v2 {

class EntityDrawDataUpdaterSystem : public System<
    ComponentDescriptor<MeshComponent, COMPONENT_RW_FLAGS_READ_WRITE>,
    ComponentDescriptor<TransformComponent, COMPONENT_RW_FLAGS_READ>,
    ComponentDescriptor<BoundingBoxComponent, COMPONENT_RW_FLAGS_READ>
>
{
public:
    virtual ~EntityDrawDataUpdaterSystem() override = default;

    virtual void Process(EntityManager &entity_manager, GameCounter::TickUnit delta) override;
};

} // namespace hyperion::v2

#endif