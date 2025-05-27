/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */
#ifndef NOISE_FACTORY_HPP
#define NOISE_FACTORY_HPP

#include <util/random/Simplex.hpp>
#include <util/random/WorleyNoise.hpp>
#include <util/img/Bitmap.hpp>

#include <core/Defines.hpp>

#include <core/math/Vector2.hpp>
#include <core/math/Vector3.hpp>

#include <core/containers/SortedArray.hpp>
#include <core/memory/UniquePtr.hpp>
#include <core/utilities/Range.hpp>

#include <Types.hpp>

#include <map>
#include <utility>
#include <random>

namespace hyperion {

using Seed = uint32;

enum NoiseGenerationType
{
    BASIC_NOISE,
    SIMPLEX_NOISE,
    WORLEY_NOISE
};

class HYP_API NoiseGenerator
{
    friend class NoiseFactory;

public:
    NoiseGenerator(NoiseGenerationType type, Seed seed);
    virtual ~NoiseGenerator() = default;

    Seed GetSeed() const
    {
        return m_seed;
    }

    virtual double GetNoise(double x, double z) const = 0;

    virtual double GetNoise(double x, double y, double z) const
    {
        return GetNoise(x, y);
    }

    double GetNoise(const Vec2f& xy) const
    {
        return GetNoise(xy.x, xy.y);
    }

    double GetNoise(const Vec3f& xyz) const
    {
        return GetNoise(xyz.x, xyz.y, xyz.z);
    }

    Bitmap<1> CreateBitmap(uint32 width, uint32 height, float scale = 1.0f);

protected:
    Seed m_seed;

private:
    NoiseGenerationType m_type;
};

template <class T>
class BasicNoiseGenerator
{
public:
    BasicNoiseGenerator(Seed seed, const Range<T>& range)
        : m_seed(seed),
          m_range(range),
          m_mt { m_random_device() },
          m_distribution { range.GetStart(), range.GetEnd() }
    {
    }

    BasicNoiseGenerator(const BasicNoiseGenerator& other) = delete;
    BasicNoiseGenerator& operator=(const BasicNoiseGenerator& other) = delete;
    BasicNoiseGenerator(BasicNoiseGenerator&& other) noexcept = default;
    BasicNoiseGenerator& operator=(BasicNoiseGenerator&& other) noexcept = default;
    virtual ~BasicNoiseGenerator() = default;

    virtual T Next()
    {
        return m_distribution(m_mt);
    }

protected:
    Seed m_seed;

    Range<T> m_range;

    std::random_device m_random_device;
    std::mt19937 m_mt;
    std::uniform_real_distribution<T> m_distribution;
};

class HYP_API SimplexNoiseGenerator : public NoiseGenerator
{
public:
    SimplexNoiseGenerator(Seed seed);
    virtual ~SimplexNoiseGenerator() override;

    virtual double GetNoise(double x, double z) const override;
    virtual double GetNoise(double x, double y, double z) const override;

private:
    SimplexNoiseData m_simplex_noise;
};

class HYP_API WorleyNoiseGenerator : public NoiseGenerator
{
public:
    WorleyNoiseGenerator(Seed seed);
    virtual ~WorleyNoiseGenerator() override;

    virtual double GetNoise(double x, double z) const override;
    virtual double GetNoise(double x, double y, double z) const override;

private:
    WorleyNoise* m_worley_noise;
};

class HYP_API NoiseCombinator
{
public:
    enum class Mode
    {
        ADDITIVE,
        MULTIPLICATIVE
    };

    struct NoiseGeneratorInstance
    {
        Mode mode;
        UniquePtr<NoiseGenerator> generator;
        float multiplier; // amount to multiply a result by
        float bias;       // amount to add to a result (pre-mult)
        Vec3f scaling;    // coord scaling
    };

    NoiseCombinator()
        : m_seed(0)
    {
    }

    NoiseCombinator(Seed seed)
        : m_seed(seed)
    {
    }

    NoiseCombinator(const NoiseCombinator& other) = delete;
    NoiseCombinator& operator=(const NoiseCombinator& other) = delete;
    NoiseCombinator(NoiseCombinator&& other) noexcept = default;
    NoiseCombinator& operator=(NoiseCombinator&& other) noexcept = default;
    ~NoiseCombinator() = default;

    Seed GetSeed() const
    {
        return m_seed;
    }

    void SetSeed(Seed seed)
    {
        m_seed = seed;
    }

    template <class NoiseGeneratorType>
    NoiseCombinator& Use(
        int priority,
        Mode mode = Mode::ADDITIVE,
        float multiplier = 1.0f,
        float bias = 0.0f,
        const Vec3f& scaling = Vec3f::One())
    {
        static_assert(std::is_base_of_v<NoiseGenerator, NoiseGeneratorType>,
            "Must be a derived class of NoiseGenerator!");

        m_generators.Insert({ priority,
            NoiseGeneratorInstance {
                .mode = mode,
                .generator = MakeUnique<NoiseGeneratorType>(m_seed),
                .multiplier = multiplier,
                .bias = bias,
                .scaling = scaling } });

        return *this;
    }

    float GetNoise(const Vec2f& xy) const
    {
        float result = 0.0f;

        bool first = true;

        for (auto& it : m_generators)
        {
            ApplyNoiseValue(
                it.second.mode,
                float(it.second.generator->GetNoise(xy * Vec2f(it.second.scaling.x, it.second.scaling.y)) + it.second.bias) * it.second.multiplier,
                result,
                first);
        }

        return result;
    }

    float GetNoise(const Vec3f& xyz) const
    {
        float result = 0.0f;

        bool first = true;

        for (auto& it : m_generators)
        {
            ApplyNoiseValue(
                it.second.mode,
                float(it.second.generator->GetNoise(xyz * Vec3f(it.second.scaling)) + it.second.bias) * it.second.multiplier,
                result,
                first);
        }

        return result;
    }

protected:
    static void ApplyNoiseValue(Mode mode, float noise_value, float& final_result, bool& first)
    {
        switch (mode)
        {
        case Mode::ADDITIVE:
            final_result += noise_value;

            break;
        case Mode::MULTIPLICATIVE:
            if (first)
            {
                final_result = 1.0f;
            }

            final_result *= noise_value;

            break;
        }

        first = false;
    }

    Seed m_seed;
    SortedArray<KeyValuePair<int, NoiseGeneratorInstance>> m_generators;
};

struct NoiseGeneratorRefCounter
{
    NoiseGenerator* noise;
    SizeType uses;
};

class HYP_API NoiseFactory
{
public:
    static NoiseFactory* s_instance;
    static NoiseFactory* GetInstance();

    NoiseFactory() = default;
    ~NoiseFactory() = default;

    NoiseGenerator* Capture(NoiseGenerationType type, Seed seed);
    void Release(NoiseGenerator* noise);
    void Release(NoiseGenerationType type, Seed seed);

private:
    std::map<
        std::pair<NoiseGenerationType, Seed>,
        NoiseGeneratorRefCounter>
        m_noise_generators;
};

} // namespace hyperion

#endif
