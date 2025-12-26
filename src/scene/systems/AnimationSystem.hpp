/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <scene/System.hpp>
#include <scene/components/AnimationComponent.hpp>
#include <scene/components/MeshComponent.hpp>

#include <core/containers/HashMap.hpp>

#include <core/memory/resource/Resource.hpp>

#include <asset/AssetReference.hpp>

namespace Hyperion {

class Skeleton;
class SkeletonAsset;

HYP_CLASS(NoScriptBindings)
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

    HashMap<Skeleton*, ResourceHandle> m_resourceHandles;
};

} // namespace Hyperion
