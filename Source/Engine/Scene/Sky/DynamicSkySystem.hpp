/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <Scene/Scene.hpp>
#include <Scene/System.hpp>
#include <Scene/EnvProbe.hpp>

#include <Scene/Camera/Camera.hpp>

#include <Core/Utilities/ClockTimer.hpp>

namespace Hyperion {

HYP_CLASS()
class ENGINE_API DynamicSkySystem : public SystemBase
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
    void InitializeSky();

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
    uint32 m_lastFrame;
};

} // namespace Hyperion
