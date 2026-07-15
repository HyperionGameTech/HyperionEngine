/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <Scene/System.hpp>
#include <Scene/EntityTag.hpp>
#include <Scene/Components/RigidBodyComponent.hpp>
#include <Scene/Components/TransformComponent.hpp>

#include <Core/Containers/Map.hpp>
#include <Core/Functional/Delegate.hpp>
#include <Core/Math/Transform.hpp>

namespace Hyperion {

/*! \brief This system initialized physics objects.
 *  It does not update them, due to needing to mutate states before visibility calculation, hence this logic exists in World::SyncPhysicsToEntities() */
HYP_CLASS(NoScriptBindings)
class PhysicsSystem final : public SystemBase
{
    HYP_OBJECT_BODY(PhysicsSystem);

public:
    ~PhysicsSystem() override = default;

    bool AllowUpdate() const override
    {
        // Process() is a No-op
        return false;
    }

    void OnEntityAdded(Entity* entity) override;
    void OnEntityRemoved(Entity* entity) override;

    void Process(float delta, Span<Handle<Scene>> scenes) override;

    SystemComponentDescriptors GetComponentDescriptors() const override
    {
        return {
            ComponentDescriptor<RigidBodyComponent, ComponentAccess::READ_WRITE> {},
            ComponentDescriptor<TransformComponent, ComponentAccess::READ_WRITE> {},

            ComponentDescriptor<TagComponent<EntityTag::UpdatePhysicsShape>, ComponentAccess::READ, false> {},
            ComponentDescriptor<TagComponent<EntityTag::UpdatePhysicsMaterial>, ComponentAccess::READ, false> {}
        };
    }

}; // class PhysicsSystem

} // namespace Hyperion
