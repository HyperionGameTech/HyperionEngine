/* Copyright (c) 2016-2026 Andrew J. MacDonald. All rights reserved. */

#pragma once

#include <scene/System.hpp>
#include <scene/components/AnimationComponent.hpp>

#include <Core/containers/HashMap.hpp>

#include <Core/memory/resource/Resource.hpp>

#include <asset/AssetReference.hpp>

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

    HashMap<Skeleton*, Array<UniquePtr<TSharedLock<AssetObject>>>> m_resourceHandles;
};

} // namespace Hyperion
