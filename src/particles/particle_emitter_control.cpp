#include "particle_emitter_control.h"
#include "../math/math_util.h"
#include "../entity.h"

#include <cassert>
#include <algorithm>

namespace hyperion {
ParticleEmitterControl::ParticleEmitterControl(Camera *camera, const ParticleConstructionInfo &info)
    : EntityControl(fbom::FBOMObjectType("PARTICLE_EMITTER_CONTROL"), 60.0),
      m_camera(camera)
{
    m_particle_renderer.reset(new ParticleRenderer(info));

    m_entity.reset(new Entity("Particles"));
    m_entity->SetRenderable(m_particle_renderer);
}

void ParticleEmitterControl::ResetParticle(Particle &particle)
{
    double lifespan_random = MathUtil::EPSILON +
        MathUtil::Random(0.0, fabs(m_particle_renderer->m_info.m_lifespan_randomness));

    particle.m_origin = (((m_particle_renderer->m_info.m_origin + Vector3(
        MathUtil::Random(-m_particle_renderer->m_info.m_origin_randomness.GetX(), m_particle_renderer->m_info.m_origin_randomness.GetX()),
        MathUtil::Random(-m_particle_renderer->m_info.m_origin_randomness.GetY(), m_particle_renderer->m_info.m_origin_randomness.GetY()),
        MathUtil::Random(-m_particle_renderer->m_info.m_origin_randomness.GetZ(), m_particle_renderer->m_info.m_origin_randomness.GetZ())
    )))) * m_entity->GetGlobalTransform().GetMatrix();
    particle.m_position = particle.m_origin;
    particle.m_global_position = particle.m_position;//parent->GetGlobalTransform().GetTranslation() + particle.m_position;
    particle.m_scale = m_particle_renderer->m_info.m_scale + Vector3(
        MathUtil::Random(-m_particle_renderer->m_info.m_scale_randomness.GetX(), m_particle_renderer->m_info.m_scale_randomness.GetX()),
        MathUtil::Random(-m_particle_renderer->m_info.m_scale_randomness.GetY(), m_particle_renderer->m_info.m_scale_randomness.GetY()),
        MathUtil::Random(-m_particle_renderer->m_info.m_scale_randomness.GetZ(), m_particle_renderer->m_info.m_scale_randomness.GetZ())
    );
    particle.m_global_scale = parent->GetGlobalTransform().GetScale() * particle.m_scale;
    particle.m_velocity = m_particle_renderer->m_info.m_velocity + Vector3(
        MathUtil::Random(-m_particle_renderer->m_info.m_velocity_randomness.GetX(), m_particle_renderer->m_info.m_velocity_randomness.GetX()),
        MathUtil::Random(-m_particle_renderer->m_info.m_velocity_randomness.GetY(), m_particle_renderer->m_info.m_velocity_randomness.GetY()),
        MathUtil::Random(-m_particle_renderer->m_info.m_velocity_randomness.GetZ(), m_particle_renderer->m_info.m_velocity_randomness.GetZ())
    );
    particle.m_camera_distance = 0.0;
    particle.m_mass = m_particle_renderer->m_info.m_mass + MathUtil::Random(
        -m_particle_renderer->m_info.m_mass_randomness,
        m_particle_renderer->m_info.m_mass_randomness
    );
    particle.m_life = 0.0;
    particle.m_lifespan = m_particle_renderer->m_info.m_lifespan + lifespan_random;
    particle.m_alive = true;
}

void ParticleEmitterControl::OnAdded()
{
    m_particle_renderer->m_particles = &m_particles;

    // add all particles
    m_particles.resize(m_particle_renderer->m_info.m_max_particles);
    for (Particle &particle : m_particles) {
        ResetParticle(particle);
    }

    m_entity->SetMaterial(parent->GetMaterial()); // clone material
    parent->AddChild(m_entity);
}

void ParticleEmitterControl::OnRemoved()
{
    m_particle_renderer->m_particles = nullptr;

    parent->RemoveChild(m_entity);
}

void ParticleEmitterControl::OnUpdate(double dt)
{
    AssertThrow(m_particles.size() <= m_particle_renderer->m_info.m_max_particles);

    for (Particle &particle : m_particles) {
        if (particle.m_alive) {
            particle.m_life += dt;
            particle.m_velocity += m_particle_renderer->m_info.m_gravity * particle.m_mass * dt;
            particle.m_position += particle.m_velocity * dt;
            particle.m_global_position = particle.m_position;
            particle.m_camera_distance = particle.m_global_position.Distance(m_camera->GetTranslation());
        } else {
            // reset the particle if it is passed it's lifespan.
            ResetParticle(particle);
        }

        if (particle.m_life >= particle.m_lifespan) {
            // particle has passed it's lifespan, no longer alive
            particle.m_alive = false;
        }
    }

    // sort particles so that the closest particles are rendered first
    // TODO: use more efficient algo
    std::sort(m_particles.begin(), m_particles.end());
}

std::shared_ptr<EntityControl> ParticleEmitterControl::CloneImpl()
{
    AssertThrow(m_particle_renderer != nullptr);

    auto clone = std::make_shared<ParticleEmitterControl>(nullptr, m_particle_renderer->m_info); // TODO

    clone->m_particles = m_particles;

    return clone;
}
} // namespace hyperion
