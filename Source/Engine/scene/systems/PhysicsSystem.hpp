/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <scene/System.hpp>
#include <scene/EntityTag.hpp>
#include <scene/components/RigidBodyComponent.hpp>
#include <scene/components/TransformComponent.hpp>

namespace Hyperion {

/*! \brief System for updating transforms of objects with RigidBodyComponent to sync with physics simulation.
 *
 * This system processes entities with RigidBodyComponent and TransformComponent,
 * updating their transforms based on the physics simulation results. */
HYP_CLASS(NoScriptBindings)
class PhysicsSystem : public SystemBase
{
    HYP_OBJECT_BODY(PhysicsSystem);

public:
    virtual ~PhysicsSystem() override = default;

    virtual bool ShouldProcessScene(Scene* scene) const override;

    virtual void OnEntityAdded(Entity* entity) override;
    virtual void OnEntityRemoved(Entity* entity) override;

    virtual void Process(float delta, Span<Handle<Scene>> scenes) override;

private:
    virtual SystemComponentDescriptors GetComponentDescriptors() const override
    {
        return {
            ComponentDescriptor<RigidBodyComponent, ComponentAccess::READ_WRITE> {},
            ComponentDescriptor<TransformComponent, ComponentAccess::READ_WRITE> {},

            ComponentDescriptor<TagComponent<EntityTag::UpdatePhysicsShape>, ComponentAccess::READ, false> {},
            ComponentDescriptor<TagComponent<EntityTag::UpdatePhysicsMaterial>, ComponentAccess::READ, false> {}
        };
    }
};

} // namespace Hyperion
