/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_SKYDOME_RENDERER_HPP
#define HYPERION_SKYDOME_RENDERER_HPP

#include <core/Base.hpp>

#include <core/object/HypObject.hpp>

#include <rendering/PostFX.hpp>
#include <rendering/RenderSubsystem.hpp>

#include <rendering/backend/RenderObject.hpp>

#include <scene/Scene.hpp>
#include <scene/EnvProbe.hpp>
#include <scene/camera/Camera.hpp>

namespace hyperion {

HYP_CLASS()
class HYP_API SkydomeRenderer : public RenderSubsystem
{
    HYP_OBJECT_BODY(SkydomeRenderer);

public:
    SkydomeRenderer(Name name, Vec2u dimensions = { 1024, 1024 });
    virtual ~SkydomeRenderer() override = default;

    HYP_FORCE_INLINE const Handle<Texture>& GetCubemap() const
    {
        return m_cubemap;
    }

    HYP_FORCE_INLINE const Handle<EnvProbe>& GetEnvProbe() const
    {
        return m_env_probe;
    }

private:
    virtual void Init() override;
    virtual void InitGame() override;
    virtual void OnRemoved() override;
    virtual void OnUpdate(GameCounter::TickUnit delta) override;
    virtual void OnRender(FrameBase* frame, const RenderSetup& render_setup) override;

    Vec2u m_dimensions;
    Handle<Texture> m_cubemap;
    Handle<Camera> m_camera;

    Handle<Scene> m_virtual_scene;
    Handle<EnvProbe> m_env_probe;
};

} // namespace hyperion

#endif