/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_V2_REFLECTION_PROBE_HPP
#define HYPERION_V2_REFLECTION_PROBE_HPP

#include <core/Base.hpp>
#include <rendering/PostFX.hpp>
#include <rendering/RenderGroup.hpp>
#include <rendering/Light.hpp>
#include <rendering/RenderComponent.hpp>
#include <rendering/Buffers.hpp>
#include <rendering/EnvProbe.hpp>

#include <rendering/backend/RendererFrame.hpp>

#include <core/lib/FixedArray.hpp>

#include <math/BoundingBox.hpp>
#include <scene/Scene.hpp>
#include <scene/camera/Camera.hpp>
#include <Types.hpp>

#include <array>

namespace hyperion::v2 {

using renderer::Frame;
using renderer::Image;
using renderer::ImageView;

class HYP_API ReflectionProbeRenderer : public RenderComponent<ReflectionProbeRenderer>
{
public:
    ReflectionProbeRenderer(
        Name name,
        const Vector3 &origin
    );

    ReflectionProbeRenderer(
        Name name,
        const BoundingBox &aabb
    );

    ReflectionProbeRenderer(const ReflectionProbeRenderer &other)               = delete;
    ReflectionProbeRenderer &operator=(const ReflectionProbeRenderer &other)    = delete;
    virtual ~ReflectionProbeRenderer();

    const Handle<EnvProbe> &GetEnvProbe() const
        { return m_env_probe; }

    void Init();
    void InitGame(); // init on game thread
    void OnRemoved();

    void OnUpdate(GameCounter::TickUnit delta);
    void OnRender(Frame *frame);

private:
    virtual void OnComponentIndexChanged(RenderComponentBase::Index new_index, RenderComponentBase::Index prev_index) override;

    BoundingBox         m_aabb;
    Handle<EnvProbe>    m_env_probe;

    bool                m_last_visibility_state = false;
};


} // namespace hyperion::v2

#endif
