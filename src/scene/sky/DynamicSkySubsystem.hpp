/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <rendering/RenderObject.hpp>

#include <scene/Scene.hpp>
#include <scene/EnvProbe.hpp>
#include <scene/Subsystem.hpp>

#include <scene/camera/Camera.hpp>

#include <util/GameCounter.hpp>

namespace Hyperion {

HYP_CLASS()
class HYP_API DynamicSkySubsystem : public Subsystem
{
    HYP_OBJECT_BODY(DynamicSkySubsystem);

public:
    DynamicSkySubsystem();
    explicit DynamicSkySubsystem(Vec2u dimensions);
    virtual ~DynamicSkySubsystem() override;

    HYP_FORCE_INLINE const Handle<Texture>& GetCubemap() const
    {
        return m_cubemap;
    }

    HYP_FORCE_INLINE const Handle<EnvProbe>& GetEnvProbe() const
    {
        return m_envProbe;
    }

    virtual void OnAddedToWorld() override;
    virtual void OnRemovedFromWorld() override;
    virtual void OnSceneAttached(const Handle<Scene>& scene) override;
    virtual void OnSceneDetached(Scene* scene) override;
    virtual void Update(float delta) override;

private:
    virtual void Init() override;

    Vec2u m_dimensions;
    Handle<Texture> m_cubemap;

    // For rendering the sky into the cubemap
    Handle<Camera> m_camera;
    Handle<Scene> m_renderScene;
    Handle<EnvProbe> m_envProbe;

    // Stuff that gets added to world
    Handle<Entity> m_skyboxEntity;
    Handle<Scene> m_visScene;

    LockstepGameCounter m_updateTimer;
};

} // namespace Hyperion
