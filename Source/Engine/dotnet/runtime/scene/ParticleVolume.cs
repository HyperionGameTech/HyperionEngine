using System;
using System.Runtime.InteropServices;

namespace Hyperion
{
    [ClassBinding(Name = "ParticleVolumeParams")]
    public struct ParticleVolumeParams
    {
        Handle<Texture> texture;
        uint maxParticles = 256u;
        Vec3f origin = Vec3f.Zero;
        float startSize = 0.035f;
        float radius = 1.0f;
        float randomness = 0.5f;
        float lifespan = 1.0f;
        bool hasPhysics = false;

        public ParticleVolumeParams()
        {
        }
    }

    [ClassBinding(Name = "ParticleVolume")]
    public class ParticleVolume : VolumeBase
    {
        public ParticleVolume()
        {
        }
    }
}