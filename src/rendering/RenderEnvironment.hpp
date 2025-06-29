/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_ENVIRONMENT_HPP
#define HYPERION_ENVIRONMENT_HPP

#include <rendering/ParticleSystem.hpp>
#include <rendering/GaussianSplatting.hpp>
#include <rendering/rt/RaytracingReflections.hpp>
#include <rendering/rt/DDGI.hpp>

#include <rendering/backend/RenderObject.hpp>

#include <core/Base.hpp>
#include <core/Name.hpp>

#include <core/utilities/Pair.hpp>

#include <core/math/MathUtil.hpp>
#include <Constants.hpp>
#include <Types.hpp>

namespace hyperion {

class Engine;
class RenderWorld;
class RenderView;

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

    const FixedArray<TLASRef, max_frames_in_flight>& GetTopLevelAccelerationStructures() const
    {
        return m_top_level_acceleration_structures;
    }

    const Handle<ParticleSystem>& GetParticleSystem() const
    {
        return m_particle_system;
    }

    const Handle<GaussianSplatting>& GetGaussianSplatting() const
    {
        return m_gaussian_splatting;
    }

    void Initialize();

    void RenderRTRadiance(FrameBase* frame, const RenderSetup& render_setup);
    void RenderDDGIProbes(FrameBase* frame, const RenderSetup& render_setup);

private:
    HYP_FORCE_INLINE void AddUpdateMarker(RenderEnvironmentUpdates value, ThreadType thread_type)
    {
        m_update_marker.BitOr(value << (RENDER_ENVIRONMENT_UPDATES_THREAD_MASK * uint64(thread_type)), MemoryOrder::RELEASE);
    }

    HYP_FORCE_INLINE void RemoveUpdateMarker(RenderEnvironmentUpdates value, ThreadType thread_type)
    {
        m_update_marker.BitAnd(~(value << (RENDER_ENVIRONMENT_UPDATES_THREAD_MASK * uint64(thread_type))), MemoryOrder::RELEASE);
    }

    HYP_FORCE_INLINE bool GetUpdateMarker(RenderEnvironmentUpdates value, ThreadType thread_type) const
    {
        return m_update_marker.Get(MemoryOrder::ACQUIRE) & (value << (RENDER_ENVIRONMENT_UPDATES_THREAD_MASK * uint64(thread_type)));
    }

    void ApplyTLASUpdates(FrameBase* frame, RTUpdateStateFlags flags);

    void InitializeRT();
    bool CreateTopLevelAccelerationStructures();

    AtomicVar<RenderEnvironmentUpdates> m_update_marker { RENDER_ENVIRONMENT_UPDATES_NONE };

    Handle<ParticleSystem> m_particle_system;

    Handle<GaussianSplatting> m_gaussian_splatting;

    UniquePtr<RaytracingReflections> m_rt_radiance;
    DDGI m_ddgi;
    bool m_has_rt_radiance;
    bool m_has_ddgi_probes;
    bool m_rt_initialized;
    FixedArray<TLASRef, max_frames_in_flight> m_top_level_acceleration_structures;
};

} // namespace hyperion

#endif