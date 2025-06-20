/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_POINT_LIGHT_SHADOW_RENDERER_HPP
#define HYPERION_POINT_LIGHT_SHADOW_RENDERER_HPP

#include <core/Base.hpp>
#include <core/Handle.hpp>

#include <core/object/HypObject.hpp>

#include <core/memory/resource/Resource.hpp>

#include <rendering/RenderSubsystem.hpp>

#include <rendering/backend/RenderObject.hpp>

#include <core/math/BoundingBox.hpp>

namespace hyperion {

class Light;
class EnvProbe;
class Scene;
class RenderScene;
class RenderShadowMap;
class RenderLight;

HYP_CLASS(NoScriptBindings)
class HYP_API PointLightShadowRenderer : public RenderSubsystem
{
    HYP_OBJECT_BODY(PointLightShadowRenderer);

public:
    PointLightShadowRenderer(Name name, const Handle<Scene>& parent_scene, const Handle<Light>& light, const Vec2u& extent);

    PointLightShadowRenderer(const PointLightShadowRenderer& other) = delete;
    PointLightShadowRenderer& operator=(const PointLightShadowRenderer& other) = delete;
    virtual ~PointLightShadowRenderer() override;

    virtual void OnRemoved() override;
    virtual void OnUpdate(float delta) override;

private:
    virtual void Init() override;

    Handle<Scene> m_parent_scene;
    Handle<Light> m_light;
    Vec2u m_extent;
    BoundingBox m_aabb;
    Handle<EnvProbe> m_env_probe;

    TResourceHandle<RenderShadowMap> m_shadow_map;
};

} // namespace hyperion

#endif
