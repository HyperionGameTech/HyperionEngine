/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <Scene/Scene.hpp>
#include <Scene/System.hpp>
#include <Scene/EnvProbe.hpp>

#include <Scene/Sky/CloudEffectVolume.hpp>

#include <Scene/Camera/Camera.hpp>

#include <Core/Utilities/ClockTimer.hpp>

#include <Core/Functional/Delegate.hpp>

namespace Hyperion {

struct EnvironmentSettings;

HYP_CLASS()
class ENGINE_API DynamicSkySystem : public SystemBase
{
    HYP_OBJECT_BODY(DynamicSkySystem);

public:
    // one texel of the sky visibility map covers SkyVisibilityWorldExtent / SkyVisibilityMapDimensions meters
    static constexpr uint32 SkyVisibilityMapDimensions = 1024;
    static constexpr float SkyVisibilityWorldExtent = 512.0f;

    // how far above the viewer the capture starts, and how deep it reaches below that
    static constexpr float SkyVisibilityHeightAboveViewer = 400.0f;
    static constexpr float SkyVisibilityDepthRange = 1200.0f;

    DynamicSkySystem();
    virtual ~DynamicSkySystem() override;

    HYP_FORCE_INLINE const Handle<Texture>& GetCubemap() const
    {
        return m_cubemap;
    }

    HYP_FORCE_INLINE const Handle<EnvProbe>& GetEnvProbe() const
    {
        return m_envProbe;
    }

    HYP_FORCE_INLINE const Handle<CloudEffectVolume>& GetCloudEffectVolume() const
    {
        return m_cloudEffectVolume;
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

    void HandleWorldEnvironmentSettingsChanged(const EnvironmentSettings& environmentSettings);

    // Centers the top-down capture on the viewer and rebuilds its matrices. Sim thread only.
    void UpdateSkyVisibilityView();

    virtual SystemComponentDescriptors GetComponentDescriptors() const override
    {
        return { };
    }

    Handle<Texture> m_cubemap;

    // For rendering the sky into the cubemap
    Handle<Camera> m_camera;
    Handle<Scene> m_renderScene;
    Handle<EnvProbe> m_envProbe;

    // Top-down capture of what blocks the sky, used to occlude sky light under canopies
    Handle<Camera> m_skyVisibilityCamera;
    Handle<View> m_skyVisibilityView;

    // Stuff that gets added to world
    Handle<Entity> m_skyboxEntity;
    Handle<Scene> m_visScene;

    Handle<CloudEffectVolume> m_cloudEffectVolume;

    ClockTimer m_updateTimer;
    uint32 m_lastFrame;

    DelegateHandler m_onWorldEnvironmentSettingsChangedHandler;
};

} // namespace Hyperion
