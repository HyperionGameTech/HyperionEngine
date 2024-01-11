#ifndef HYPERION_V2_ECS_WORLD_AABB_UPDATER_SYSTEM_HPP
#define HYPERION_V2_ECS_WORLD_AABB_UPDATER_SYSTEM_HPP

#include <scene/ecs/System.hpp>
#include <scene/ecs/components/TransformComponent.hpp>
#include <scene/ecs/components/BoundingBoxComponent.hpp>
#include <scene/ecs/components/MeshComponent.hpp>
#include <rendering/EntityDrawData.hpp>

namespace hyperion::v2 {

class WorldAABBUpdaterSystem : public System<
    ComponentDescriptor<BoundingBoxComponent, COMPONENT_RW_FLAGS_READ_WRITE>,
    ComponentDescriptor<TransformComponent, COMPONENT_RW_FLAGS_READ>,
    ComponentDescriptor<MeshComponent, COMPONENT_RW_FLAGS_READ_WRITE>
>
{
public:
    virtual ~WorldAABBUpdaterSystem() override = default;

    virtual void Process(EntityManager &entity_manager, GameCounter::TickUnit delta) override;
};

} // namespace hyperion::v2

#endif