#include "noise_factory.h"

#include "random/simplex.h"
#include "../util.h"

#include <cmath>
#include <iostream>

namespace hyperion {

NoiseGenerator::NoiseGenerator(NoiseGenerationType type, Seed_t seed)
    : m_type(type),
      m_seed(seed)
{
}

// simplex

SimplexNoiseGenerator::SimplexNoiseGenerator(Seed_t seed)
    : NoiseGenerator(NoiseGenerationType::SIMPLEX_NOISE, seed)
{
    for (int i = 0; i < OSN_OCTAVE_COUNT; i++) {
        open_simplex_noise(seed, &m_simplex_noise.octaves[i]);
        m_simplex_noise.frequencies[i] = pow(2.0, double(i));
        m_simplex_noise.amplitudes[i] = pow(0.5, OSN_OCTAVE_COUNT - i);
    }
}

SimplexNoiseGenerator::~SimplexNoiseGenerator()
{
    for (int i = 0; i < OSN_OCTAVE_COUNT; i++) {
        open_simplex_noise_free(m_simplex_noise.octaves[i]);
    }
}

double SimplexNoiseGenerator::GetNoise(double x, double z) const
{
    double result = 0.0;

    for (int i = 0; i < OSN_OCTAVE_COUNT; i++) {
        result += open_simplex_noise2(m_simplex_noise.octaves[i], x / m_simplex_noise.frequencies[i], z / m_simplex_noise.frequencies[i]) * m_simplex_noise.amplitudes[i];
    }

    return result;
}

// worley

WorleyNoiseGenerator::WorleyNoiseGenerator(Seed_t seed)
    : NoiseGenerator(NoiseGenerationType::WORLEY_NOISE, seed)
{
    m_worley_noise = new WorleyNoise(seed);
}

WorleyNoiseGenerator::~WorleyNoiseGenerator()
{
    delete m_worley_noise;
}

double WorleyNoiseGenerator::GetNoise(double x, double z) const
{
    return m_worley_noise->Noise(x, z, 0);
}

// factory

NoiseFactory *NoiseFactory::instance = nullptr;

NoiseFactory *NoiseFactory::GetInstance()
{
    if (instance == nullptr) {
        instance = new NoiseFactory();
    }

    return instance;
}

NoiseGenerator *NoiseFactory::Capture(NoiseGenerationType type, Seed_t seed)
{
    const auto type_and_seed = std::make_pair(type, seed);
    auto it = m_noise_generators.find(type_and_seed);

    if (it == m_noise_generators.end()) {
        NoiseGeneratorRefCounter ref;

        switch (type) {
        case SIMPLEX_NOISE:
            ref.noise = new SimplexNoiseGenerator(seed);
            break;
        case WORLEY_NOISE:
            ref.noise = new WorleyNoiseGenerator(seed);
            break;
        default:
            AssertThrowMsg(false, "Unsupported noise type");
        }

        ref.uses = 1;

        m_noise_generators[type_and_seed] = ref;

        return ref.noise;
    }

    it->second.uses++;

    return it->second.noise;
}

void NoiseFactory::Release(NoiseGenerator *noise)
{
    AssertThrow(noise != nullptr);

    Release(noise->m_type, noise->m_seed);
}

void NoiseFactory::Release(NoiseGenerationType type, Seed_t seed)
{
    auto it = m_noise_generators.find(std::make_pair(type, seed));

    AssertExit(it != m_noise_generators.end());

    if (!(--it->second.uses)) {
        delete it->second.noise;

        m_noise_generators.erase(it);
    }
}


} // namespace hyperion
