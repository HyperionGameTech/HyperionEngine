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
class AnimationSystem : public SystemBase
{
    HYP_OBJECT_BODY(AnimationSystem);

public:
    virtual ~AnimationSystem() override = default;

    virtual bool ShouldProcessScene(Scene* scene) const override;

    virtual void OnEntityAdded(Entity* entity) override;
    virtual void OnEntityRemoved(Entity* entity) override;

    virtual void Process(float delta, Span<Handle<Scene>> scenes) override;

private:
    virtual SystemComponentDescriptors GetComponentDescriptors() const override
    {
        return {
            ComponentDescriptor<AnimationComponent, ComponentAccess::READ_WRITE> {},
            ComponentDescriptor<MeshComponent, ComponentAccess::READ> {}
        };
    }

    TMap<Skeleton*, Array<UniquePtr<TSharedResLock<AssetObject>>, SceneAllocator>, SceneAllocator> m_resourceHandles;
};

} // namespace Hyperion
