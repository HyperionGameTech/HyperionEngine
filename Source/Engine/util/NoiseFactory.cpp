/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#include <HyperionPch.hpp>

#include <util/NoiseFactory.hpp>
#include <util/random/Simplex.hpp>

#include <cmath>
#include <iostream>

namespace Hyperion {

NoiseGenerator::NoiseGenerator(NoiseGenerationType type, Seed seed)
    : m_type(type),
      m_seed(seed)
{
}

Bitmap_R8 NoiseGenerator::CreateBitmap(uint32 width, uint32 height, float scale)
{
    Bitmap_R8 bitmap(width, height);

    for (uint32 x = 0; x < width; x++)
    {
        for (uint32 y = 0; y < height; y++)
        {
            const auto noiseValue = GetNoise(Vector2(static_cast<float>(x), static_cast<float>(y)) * scale);
            bitmap.GetPixelReference(x, y).SetR(float(noiseValue) * 0.5f + 0.5f);
        }
    }

    return bitmap;
}

// simplex

SimplexNoiseGenerator::SimplexNoiseGenerator(Seed seed)
    : NoiseGenerator(NoiseGenerationType::SIMPLEX_NOISE, seed)
{
    for (int i = 0; i < OSN_OCTAVE_COUNT; i++)
    {
        open_simplex_noise(seed, &m_simplexNoise.octaves[i]);
        m_simplexNoise.frequencies[i] = pow(2.0, double(i));
        m_simplexNoise.amplitudes[i] = pow(0.5, OSN_OCTAVE_COUNT - i);
    }
}

SimplexNoiseGenerator::~SimplexNoiseGenerator()
{
    for (int i = 0; i < OSN_OCTAVE_COUNT; i++)
    {
        open_simplex_noise_free(m_simplexNoise.octaves[i]);
    }
}

double SimplexNoiseGenerator::GetNoise(double x, double z) const
{
    double result = 0.0;

    for (int i = 0; i < OSN_OCTAVE_COUNT; i++)
    {
        result += open_simplex_noise2(m_simplexNoise.octaves[i], x / m_simplexNoise.frequencies[i], z / m_simplexNoise.frequencies[i]) * m_simplexNoise.amplitudes[i];
    }

    return result;
}

double SimplexNoiseGenerator::GetNoise(double x, double y, double z) const
{
    double result = 0.0;

    for (int i = 0; i < OSN_OCTAVE_COUNT; i++)
    {
        result += open_simplex_noise3(m_simplexNoise.octaves[i], x / m_simplexNoise.frequencies[i], y / m_simplexNoise.frequencies[i], z / m_simplexNoise.frequencies[i]) * m_simplexNoise.amplitudes[i];
    }

    return result;
}

// worley

WorleyNoiseGenerator::WorleyNoiseGenerator(Seed seed)
    : NoiseGenerator(NoiseGenerationType::WORLEY_NOISE, seed)
{
    m_worleyNoise = new WorleyNoise(seed);
}

WorleyNoiseGenerator::~WorleyNoiseGenerator()
{
    delete m_worleyNoise;
}

double WorleyNoiseGenerator::GetNoise(double x, double z) const
{
    return m_worleyNoise->Noise(x, 0.0, z);
}

double WorleyNoiseGenerator::GetNoise(double x, double y, double z) const
{
    return m_worleyNoise->Noise(x, y, z);
}

// factory

NoiseFactory* NoiseFactory::s_instance = nullptr;

NoiseFactory* NoiseFactory::GetInstance()
{
    if (s_instance == nullptr)
    {
        s_instance = new NoiseFactory();
    }

    return s_instance;
}

NoiseGenerator* NoiseFactory::Capture(NoiseGenerationType type, Seed seed)
{
    const auto typeAndSeed = std::make_pair(type, seed);
    auto it = m_noiseGenerators.find(typeAndSeed);

    if (it == m_noiseGenerators.end())
    {
        NoiseGeneratorRefCounter ref;

        switch (type)
        {
        case SIMPLEX_NOISE:
            ref.noise = new SimplexNoiseGenerator(seed);
            break;
        case WORLEY_NOISE:
            ref.noise = new WorleyNoiseGenerator(seed);
            break;
        default:
            HYP_UNREACHABLE();
        }

        ref.uses = 1;

        m_noiseGenerators[typeAndSeed] = ref;

        return ref.noise;
    }

    it->second.uses++;

    return it->second.noise;
}

void NoiseFactory::Release(NoiseGenerator* noise)
{
    Assert(noise != nullptr);

    Release(noise->m_type, noise->m_seed);
}

void NoiseFactory::Release(NoiseGenerationType type, Seed seed)
{
    const auto it = m_noiseGenerators.find(std::make_pair(type, seed));

    Assert(it != m_noiseGenerators.end());

    if (!--it->second.uses)
    {
        delete it->second.noise;

        m_noiseGenerators.erase(it);
    }
}

} // namespace Hyperion
