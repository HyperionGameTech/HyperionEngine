/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */
#ifndef HYPERION_REFLECTION_PROBE_HPP
#define HYPERION_REFLECTION_PROBE_HPP

#include <core/Base.hpp>

#include <core/object/HypObject.hpp>

#include <rendering/PostFX.hpp>
#include <rendering/Light.hpp>
#include <rendering/RenderComponent.hpp>
#include <rendering/EnvProbe.hpp>

#include <rendering/backend/RenderObject.hpp>

#include <math/BoundingBox.hpp>

namespace hyperion {

HYP_CLASS()
class HYP_API ReflectionProbeRenderer : public RenderComponentBase
{
    HYP_OBJECT_BODY(ReflectionProbeRenderer);
    
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

    HYP_FORCE_INLINE const Handle<EnvProbe> &GetEnvProbe() const
        { return m_env_probe; }

private:
    virtual void Init() override;
    virtual void InitGame() override; // init on game thread
    virtual void OnRemoved() override;
    virtual void OnUpdate(GameCounter::TickUnit delta) override;
    virtual void OnRender(Frame *frame) override;

    virtual void OnComponentIndexChanged(RenderComponentBase::Index new_index, RenderComponentBase::Index prev_index) override;

    BoundingBox         m_aabb;
    Handle<EnvProbe>    m_env_probe;

    bool                m_last_visibility_state = false;
};


} // namespace hyperion

#endif
