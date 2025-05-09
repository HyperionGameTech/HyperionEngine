/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_RENDERER_HPP
#define HYPERION_RENDERER_HPP

#include <core/config/Config.hpp>

#include <rendering/backend/RenderObject.hpp>

namespace hyperion {

HYP_STRUCT(ConfigName="app", ConfigPath="rendering")
struct RendererConfig : public ConfigBase<RendererConfig>
{
    HYP_FIELD(ConfigPath="rt.path_tracing.enabled")
    bool    path_tracer_enabled = false;

    HYP_FIELD(ConfigPath="rt.reflections.enabled")
    bool    rt_reflections_enabled = false;

    HYP_FIELD(ConfigPath="rt.gi.enabled")
    bool    rt_gi_enabled = false;

    HYP_FIELD(ConfigPath="hbao.enabled")
    bool    hbao_enabled = false;

    HYP_FIELD(ConfigPath="hbil.enabled")
    bool    hbil_enabled = false;

    HYP_FIELD(ConfigPath="ssgi.enabled")
    bool    ssgi_enabled = false;

    HYP_FIELD(ConfigPath="env_grid.gi.enabled")
    bool    env_grid_gi_enabled = false;

    HYP_FIELD(ConfigPath="env_grid.reflections.enabled")
    bool    env_grid_radiance_enabled = false;

    HYP_FIELD(ConfigPath="taa.enabled")
    bool    taa_enabled = false;

    virtual ~RendererConfig() override = default;
};

} // namespace hyperion

#endif
