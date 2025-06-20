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

class EnvProbe;

HYP_CLASS(NoScriptBindings)
class HYP_API ReflectionProbeRenderer : public RenderSubsystem
{
    HYP_OBJECT_BODY(ReflectionProbeRenderer);

public:
    ReflectionProbeRenderer(Name name, const Handle<EnvProbe>& env_probe);

    ReflectionProbeRenderer(const ReflectionProbeRenderer& other) = delete;
    ReflectionProbeRenderer& operator=(const ReflectionProbeRenderer& other) = delete;
    virtual ~ReflectionProbeRenderer();

private:
    virtual void Init() override;
    virtual void OnRemoved() override;
    virtual void OnUpdate(float delta) override;

    Handle<EnvProbe> m_env_probe;
};

} // namespace hyperion

#endif
