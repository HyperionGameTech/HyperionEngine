/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#pragma once

#include <Scene/System.hpp>
#include <Scene/EntityTag.hpp>

#include <Scene/Instancing/InstanceGroup.hpp>

namespace Hyperion {

class Prefab;

HYP_CLASS(NoScriptBindings, Serialize = false)
class InstanceGroupSystem final : public SystemBase
{
    HYP_OBJECT_BODY(InstanceGroupSystem);

public:
    ~InstanceGroupSystem() override = default;

    bool RequiresSimThread() const override
    {
        return true;
    }

    bool ShouldProcessScene(Scene* scene) const override;

    void OnAddedToWorld(World* world) override;

    void OnEntityAdded(Entity* entity) override;
    void OnEntityRemoved(Entity* entity) override;

    void Process(float delta, Span<Handle<Scene>> scenes) override;

private:
    SystemComponentDescriptors GetComponentDescriptors() const override
    {
        return {
            ComponentDescriptor<EntityType<InstanceGroup>, ComponentAccess::READ_WRITE> {},

            ComponentDescriptor<TagComponent<EntityTag::UpdateInstanceGroup>, ComponentAccess::READ, false> {}
        };
    }

    void OnPrefabChanged(Prefab* prefab);

    void RemoveReleasedEntities();

    Array<WeakHandle<InstanceGroup>, SceneAllocator> m_groupsToUpdate;
    Array<Handle<Entity>, SceneAllocator> m_releasedEntities;
};

} // namespace Hyperion
