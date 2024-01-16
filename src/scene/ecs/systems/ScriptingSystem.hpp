#ifndef HYPERION_V2_ECS_SCRIPTING_SYSTEM_HPP
#define HYPERION_V2_ECS_SCRIPTING_SYSTEM_HPP

#include <scene/ecs/System.hpp>
#include <scene/ecs/components/ScriptComponent.hpp>

namespace hyperion::v2 {

class ScriptingSystem : public System<
    ComponentDescriptor<ScriptComponent, COMPONENT_RW_FLAGS_READ_WRITE>
>
{
public:
    virtual ~ScriptingSystem() override = default;

    virtual void Process(EntityManager &entity_manager, GameCounter::TickUnit delta) override;
};

} // namespace hyperion::v2

#endif