/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */
#ifndef HYPERION_REFLECTION_PROBE_HPP
#define HYPERION_REFLECTION_PROBE_HPP

#include <core/Base.hpp>

#include <core/memory/resource/Resource.hpp>

#include <core/math/BoundingBox.hpp>

#include <core/object/HypObject.hpp>

#include <rendering/RenderSubsystem.hpp>

#include <rendering/backend/RenderObject.hpp>

namespace hyperion {

class RenderEnvProbe;

HYP_CLASS()
class HYP_API ReflectionProbeRenderer : public RenderSubsystem
{
    HYP_OBJECT_BODY(ReflectionProbeRenderer);

public:
    ReflectionProbeRenderer(
        Name name,
        const TResourceHandle<RenderEnvProbe>& env_render_probe);

    ReflectionProbeRenderer(const ReflectionProbeRenderer& other) = delete;
    ReflectionProbeRenderer& operator=(const ReflectionProbeRenderer& other) = delete;
    virtual ~ReflectionProbeRenderer();

private:
    virtual void Init() override;
    virtual void InitGame() override; // init on game thread
    virtual void OnRemoved() override;
    virtual void OnUpdate(GameCounter::TickUnit delta) override;
    virtual void OnRender(FrameBase* frame) override;

    TResourceHandle<RenderEnvProbe> m_env_render_probe;

    bool m_last_visibility_state = false;
};

} // namespace hyperion

#endif
