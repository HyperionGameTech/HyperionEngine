/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <scene/System.hpp>
#include <scene/components/AudioComponent.hpp>
#include <scene/components/TransformComponent.hpp>

namespace hyperion {

HYP_CLASS(NoScriptBindings)
class AudioSystem : public SystemBase
{
    HYP_OBJECT_BODY(AudioSystem);

public:
    virtual ~AudioSystem() override = default;

    virtual bool ShouldProcessScene(Scene* scene) const override;

    virtual void OnEntityAdded(Entity* entity) override;

    virtual bool NeedsUpdateThisFrame() const;

    virtual void Process(float delta, Span<Handle<Scene>> scenes) override;

private:
    virtual SystemComponentDescriptors GetComponentDescriptors() const override
    {
        return {
            ComponentDescriptor<AudioComponent, ComponentRWFlags::READ_WRITE> {},
            ComponentDescriptor<TransformComponent, ComponentRWFlags::READ> {}
        };
    }
};

} // namespace hyperion
