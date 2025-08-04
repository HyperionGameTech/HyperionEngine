/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <rendering/RenderObject.hpp>

#include <core/Name.hpp>
#include <core/Handle.hpp>

#include <core/utilities/Pair.hpp>

#include <core/math/MathUtil.hpp>
#include <Constants.hpp>
#include <Types.hpp>

namespace hyperion {

class Engine;
class ParticleSystem;
class GaussianSplatting;

using RenderEnvironmentUpdates = uint64;

enum RenderEnvironmentUpdateBits : RenderEnvironmentUpdates
{
    RENDER_ENVIRONMENT_UPDATES_NONE = 0x0,
    RENDER_ENVIRONMENT_UPDATES_PLACEHOLDER = 0x2,

    RENDER_ENVIRONMENT_UPDATES_TLAS = 0x4,

    RENDER_ENVIRONMENT_UPDATES_THREAD_MASK = 0x10 // use mask shifted by ThreadType value to issue unique updates for a specific thread
};

class HYP_API RenderEnvironment final
{
public:
    RenderEnvironment();
    RenderEnvironment(const RenderEnvironment& other) = delete;
    RenderEnvironment& operator=(const RenderEnvironment& other) = delete;
    ~RenderEnvironment();

    const Handle<ParticleSystem>& GetParticleSystem() const
    {
        return m_particleSystem;
    }

    const Handle<GaussianSplatting>& GetGaussianSplatting() const
    {
        return m_gaussianSplatting;
    }

    void Initialize();

private:
    HYP_FORCE_INLINE void AddUpdateMarker(RenderEnvironmentUpdates value, ThreadType threadType)
    {
        m_updateMarker.BitOr(value << (RENDER_ENVIRONMENT_UPDATES_THREAD_MASK * uint64(threadType)), MemoryOrder::RELEASE);
    }

    HYP_FORCE_INLINE void RemoveUpdateMarker(RenderEnvironmentUpdates value, ThreadType threadType)
    {
        m_updateMarker.BitAnd(~(value << (RENDER_ENVIRONMENT_UPDATES_THREAD_MASK * uint64(threadType))), MemoryOrder::RELEASE);
    }

    HYP_FORCE_INLINE bool GetUpdateMarker(RenderEnvironmentUpdates value, ThreadType threadType) const
    {
        return m_updateMarker.Get(MemoryOrder::ACQUIRE) & (value << (RENDER_ENVIRONMENT_UPDATES_THREAD_MASK * uint64(threadType)));
    }

    AtomicVar<RenderEnvironmentUpdates> m_updateMarker { RENDER_ENVIRONMENT_UPDATES_NONE };

    Handle<ParticleSystem> m_particleSystem;

    Handle<GaussianSplatting> m_gaussianSplatting;
};

} // namespace hyperion
