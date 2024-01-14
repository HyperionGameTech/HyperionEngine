#ifndef HYPERION_V2_ECS_ANIMATION_SYSTEM_HPP
#define HYPERION_V2_ECS_ANIMATION_SYSTEM_HPP

#include <scene/ecs/System.hpp>
#include <scene/ecs/components/SkeletonComponent.hpp>
#include <scene/ecs/components/MeshComponent.hpp>

namespace hyperion::v2 {

class AnimationSystem : public System<
    ComponentDescriptor<SkeletonComponent, COMPONENT_RW_FLAGS_READ_WRITE>,
    ComponentDescriptor<MeshComponent, COMPONENT_RW_FLAGS_READ_WRITE>
>
{
public:
    virtual ~AnimationSystem() override = default;

    virtual void Process(EntityManager &entity_manager, GameCounter::TickUnit delta) override;
};

} // namespace hyperion::v2

#endif