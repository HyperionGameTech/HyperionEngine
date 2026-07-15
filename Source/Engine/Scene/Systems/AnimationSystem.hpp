/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <Scene/System.hpp>
#include <Scene/Components/AnimationComponent.hpp>

#include <Core/Containers/Map.hpp>

#include <Core/Resource/Resource.hpp>
#include <Core/Resource/ResLock.hpp>

#include <Asset/AssetReference.hpp>

#include <Framework/EngineMemory.hpp>

namespace Hyperion {

class Skeleton;

struct MeshComponent;

HYP_CLASS(NoScriptBindings, Serialize=false)
class AnimationSystem final : public SystemBase
{
    HYP_OBJECT_BODY(AnimationSystem);

public:
    ~AnimationSystem() override = default;

    bool ShouldProcessScene(Scene* scene) const override;

    void OnEntityAdded(Entity* entity) override;
    void OnEntityRemoved(Entity* entity) override;

    void Process(float delta, Span<Handle<Scene>> scenes) override;

private:
    SystemComponentDescriptors GetComponentDescriptors() const override
    {
        return {
            ComponentDescriptor<AnimationComponent, ComponentAccess::READ_WRITE> {},
            ComponentDescriptor<MeshComponent, ComponentAccess::READ> {}
        };
    }

    Map<Skeleton*, Array<UniquePtr<TSharedResLock<AssetObject>>, SceneAllocator>, SceneAllocator> m_resourceHandles;
};

} // namespace Hyperion
