/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <Scene/System.hpp>
#include <Scene/Components/MeshComponent.hpp>

#include <Core/Containers/Map.hpp>

namespace Hyperion {

HYP_CLASS(NoScriptBindings)
class MeshSystem final : public SystemBase
{
    HYP_OBJECT_BODY(MeshSystem);

public:
    ~MeshSystem() override = default;

    bool ShouldProcessScene(Scene* scene) const override;

    void OnEntityAdded(Entity* entity) override;
    void OnEntityRemoved(Entity* entity) override;

    void Process(float delta, Span<Handle<Scene>> scenes) override;

private:
    SystemComponentDescriptors GetComponentDescriptors() const override
    {
        return {
            ComponentDescriptor<MeshComponent, ComponentAccess::READ_WRITE> {},

            ComponentDescriptor<TagComponent<EntityTag::UpdateInstancedMeshData>, ComponentAccess::READ, false> {}
        };
    }

#if HYP_EDITOR
    struct CachedInstancedMeshDataState
    {
        bool enableAutoInstancing;
    };

    // Per entity mapping from Entity* -> cached state (num instances, auto instancing enabled)
    // Currently only needed to be able to init/deinit instanced mesh data when changed in editor.
    TMap<Entity*, CachedInstancedMeshDataState, SceneAllocator> m_cachedStates;
#endif // HYP_EDITOR
};

} // namespace Hyperion
