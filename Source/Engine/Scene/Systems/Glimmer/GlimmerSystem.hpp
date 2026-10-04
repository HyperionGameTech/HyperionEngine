/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#pragma once

#include <Scene/System.hpp>

#include <Rendering/Glimmer/GlimmerGroundClipmap.hpp>
#include <Rendering/Glimmer/GlimmerChannel.hpp>

#include <Core/Memory/SharedPtr.hpp>

#include <Core/Math/Vector3.hpp>

namespace Hyperion {

class Camera;
class View;
class GlimmerChannel;

HYP_CLASS(NoScriptBindings, Serialize = false)
class ENGINE_API GlimmerSystem final : public SystemBase
{
    HYP_OBJECT_BODY(GlimmerSystem);

public:
    static constexpr float SceneVerticalHalfExtent = 512.0f;

    GlimmerSystem();
    virtual ~GlimmerSystem() override;

    virtual void OnAddedToWorld(World* world) override;
    virtual void OnRemovedFromWorld(World* world) override;

    virtual bool RequiresSimThread() const override
    {
        return true;
    }

    virtual void Process(float delta, Span<Handle<Scene>> scenes) override;

private:
    void CreateSceneView();
    void SetSceneViewActive(bool active);
    void UpdateSceneRegion(bool force);

    ////////// DEBUG //////////
    void UpdateProbeDebugRecords(float delta);
    void LogProbeStats();
    void DebugDrawProbes(const Vec3f& viewerPosition);
    ///////////////////////////

    virtual SystemComponentDescriptors GetComponentDescriptors() const override
    {
        return {};
    }

    Handle<Camera> m_sceneCamera;
    Handle<View> m_sceneView;
    bool m_isSceneViewActive;

    Vec3f m_regionCenter;
    float m_regionRadius;
    bool m_hasRegion;

    SharedPtr<GlimmerChannel> m_channel;
    GlimmerGroundClipmap m_groundClipmap;

    Array<GlimmerProbeDebugRecord> m_probeDebugRecords;
    float m_probeStatsLogTimer = 0.0f;
};

} // namespace Hyperion
