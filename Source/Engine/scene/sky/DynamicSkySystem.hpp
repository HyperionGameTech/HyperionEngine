/* Copyright (c) 2016-2026 Andrew J. MacDonald. All rights reserved. */

#pragma once

#include <scene/Scene.hpp>
#include <scene/System.hpp>
#include <scene/EnvProbe.hpp>

#include <scene/camera/Camera.hpp>

#include <Core/utilities/ClockTimer.hpp>

namespace Hyperion {

HYP_CLASS()
class HYP_API DynamicSkySystem : public SystemBase
{
    HYP_OBJECT_BODY(DynamicSkySystem);

public:
    DynamicSkySystem();
    explicit DynamicSkySystem(Vec2u dimensions);
    virtual ~DynamicSkySystem() override;

    HYP_FORCE_INLINE const Handle<Texture>& GetCubemap() const
    {
        return m_cubemap;
    }

    HYP_FORCE_INLINE const Handle<EnvProbe>& GetEnvProbe() const
    {
        return m_envProbe;
    }

    virtual void OnAddedToWorld(World* world) override;
    virtual void OnRemovedFromWorld(World* world) override;
    
    virtual bool RequiresSimThread() const override
    {
        return true;
    }

    virtual void Process(float delta, Span<Handle<Scene>> scenes) override;

private:
    virtual void Init() override;

    virtual SystemComponentDescriptors GetComponentDescriptors() const override
    {
        return { };
    }

    Vec2u m_dimensions;
    Handle<Texture> m_cubemap;

    // For rendering the sky into the cubemap
    Handle<Camera> m_camera;
    Handle<Scene> m_renderScene;
    Handle<EnvProbe> m_envProbe;

    // Stuff that gets added to world
    Handle<Entity> m_skyboxEntity;
    Handle<Scene> m_visScene;

    ClockTimer m_updateTimer;
};

} // namespace Hyperion
