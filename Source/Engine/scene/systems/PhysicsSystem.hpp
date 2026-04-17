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

#include <Core/containers/HashMap.hpp>
#include <Core/functional/Delegate.hpp>
#include <Core/math/Transform.hpp>

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
        return false;
    }

    void OnEntityAdded(Entity* entity) override;
    void OnEntityRemoved(Entity* entity) override;

    void OnAddedToWorld(World* world) override;
    void OnRemovedFromWorld(World* world) override;

    void Process(float delta, Span<Handle<Scene>> scenes) override;

private:
    void SaveSimulationOrigins();
    void RestoreSimulationOrigins();

    SystemComponentDescriptors GetComponentDescriptors() const override
    {
        return {
            ComponentDescriptor<RigidBodyComponent, ComponentAccess::READ_WRITE> {},
            ComponentDescriptor<TransformComponent, ComponentAccess::READ_WRITE> {},

            ComponentDescriptor<TagComponent<EntityTag::UpdatePhysicsShape>, ComponentAccess::READ, false> {},
            ComponentDescriptor<TagComponent<EntityTag::UpdatePhysicsMaterial>, ComponentAccess::READ, false> {}
        };
    }

    HashMap<Entity*, Transform, SceneAllocator> m_simulationOriginTransforms;

    DelegateHandlerSet m_delegateHandlers;
};

} // namespace Hyperion
