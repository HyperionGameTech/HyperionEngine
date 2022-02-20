#include "particle_construction_info.h"
#include "../util.h"

namespace hyperion {
ParticleConstructionInfo::ParticleConstructionInfo(const Vector3 &origin, const Vector3 &origin_randomness,
    const Vector3 &velocity, const Vector3 &velocity_randomness,
    double mass, double mass_randomness,
    size_t max_particles, double lifespan, double lifespan_randomness,
    const Vector3 &scale, const Vector3 &scale_randomness)
    : m_origin(origin),
      m_origin_randomness(origin_randomness),
      m_velocity(velocity),
      m_velocity_randomness(velocity_randomness),
      m_mass(mass),
      m_mass_randomness(mass_randomness),
      m_max_particles(max_particles),
      m_lifespan(lifespan),
      m_lifespan_randomness(lifespan_randomness),
      m_scale(scale),
      m_scale_randomness(scale_randomness)
{
    AssertThrow(m_lifespan > 0.0);
}

ParticleConstructionInfo::ParticleConstructionInfo(const ParticleConstructionInfo &other)
    : m_origin(other.m_origin),
      m_origin_randomness(other.m_origin_randomness),
      m_velocity(other.m_velocity),
      m_velocity_randomness(other.m_velocity_randomness),
      m_mass(other.m_mass),
      m_mass_randomness(other.m_mass_randomness),
      m_max_particles(other.m_max_particles),
      m_lifespan(other.m_lifespan),
      m_lifespan_randomness(other.m_lifespan_randomness),
      m_gravity(other.m_gravity),
      m_material(other.m_material),
      m_scale(other.m_scale),
      m_scale_randomness(other.m_scale_randomness)
{
    AssertThrow(m_lifespan > 0.0);
}
} // namespace hyperion
