/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_POINT_LIGHT_SHADOW_RENDERER_HPP
#define HYPERION_POINT_LIGHT_SHADOW_RENDERER_HPP

#include <core/Base.hpp>
#include <core/Handle.hpp>

#include <core/object/HypObject.hpp>

#include <rendering/RenderSubsystem.hpp>

#include <rendering/backend/RenderObject.hpp>

#include <core/math/BoundingBox.hpp>

namespace hyperion {

class Light;
class EnvProbe;

HYP_CLASS()
class HYP_API PointLightShadowRenderer : public RenderSubsystem
{
    HYP_OBJECT_BODY(PointLightShadowRenderer);

public:
    PointLightShadowRenderer(Name name, Handle<Light> light, const Vec2u &extent);
    PointLightShadowRenderer(const PointLightShadowRenderer &other) = delete;
    PointLightShadowRenderer &operator=(const PointLightShadowRenderer &other) = delete;
    virtual ~PointLightShadowRenderer() override;

private:
    virtual void Init() override;
    virtual void InitGame() override; // init on game thread
    virtual void OnRemoved() override;
    virtual void OnUpdate(GameCounter::TickUnit delta) override;
    virtual void OnRender(Frame *frame) override;

    virtual void OnComponentIndexChanged(RenderSubsystem::Index new_index, RenderSubsystem::Index prev_index) override;

    Handle<Light>       m_light;
    Vec2u               m_extent;
    BoundingBox         m_aabb;
    Handle<EnvProbe>    m_env_probe;

    bool                m_last_visibility_state = false;
};


} // namespace hyperion

#endif
