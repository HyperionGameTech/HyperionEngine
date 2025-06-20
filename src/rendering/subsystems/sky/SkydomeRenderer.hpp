/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_SKYDOME_RENDERER_HPP
#define HYPERION_SKYDOME_RENDERER_HPP

#include <core/Base.hpp>

#include <core/object/HypObject.hpp>

#include <rendering/backend/RenderObject.hpp>

#include <scene/Scene.hpp>
#include <scene/EnvProbe.hpp>
#include <scene/Subsystem.hpp>
#include <scene/camera/Camera.hpp>

namespace hyperion {

HYP_CLASS(NoScriptBindings)
class HYP_API SkydomeRenderer : public Subsystem
{
    HYP_OBJECT_BODY(SkydomeRenderer);

public:
    SkydomeRenderer(Vec2u dimensions = { 1024, 1024 });
    virtual ~SkydomeRenderer() override;

    HYP_FORCE_INLINE const Handle<Texture>& GetCubemap() const
    {
        return m_cubemap;
    }

    HYP_FORCE_INLINE const Handle<EnvProbe>& GetEnvProbe() const
    {
        return m_env_probe;
    }

    virtual void OnAddedToWorld() override;
    virtual void OnRemovedFromWorld() override;
    virtual void Update(float delta) override;

private:
    virtual void Init() override;

    Vec2u m_dimensions;
    Handle<Texture> m_cubemap;
    Handle<Camera> m_camera;

    Handle<Scene> m_virtual_scene;
    Handle<EnvProbe> m_env_probe;
};

} // namespace hyperion

#endif