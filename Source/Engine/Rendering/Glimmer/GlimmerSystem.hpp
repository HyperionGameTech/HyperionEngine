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

/*! \brief Sim side of Glimmer GI. Owns the world's Glimmer scene view, which collects the static scene around the viewer for the
 *  technique (sized by GlimmerTechnique::GetSceneRegionParams()), and keeps it centred on the viewer. Samples the terrain into the ground clipmap. */
HYP_CLASS(NoScriptBindings, Serialize = false)
class ENGINE_API GlimmerSystem final : public SystemBase
{
    HYP_OBJECT_BODY(GlimmerSystem);

public:
    // how far above and below the region centre static solids are collected
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

    /*! \brief Rendering.Glimmer.SWRT.DebugProbes: draws the latest probe readback around the viewer with the DebugDrawer. */
    void DebugDrawProbes(const Vec3f& viewerPosition);

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

    // drawn every frame, as debug draws only last one; replaced whenever a newer readback comes in
    Array<GlimmerProbeDebugRecord> m_probeDebugRecords;
    uint32 m_probeDebugLogCounter = 0;
};

} // namespace Hyperion
