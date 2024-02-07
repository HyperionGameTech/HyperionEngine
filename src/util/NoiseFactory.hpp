#ifndef NOISE_FACTORY_H
#define NOISE_FACTORY_H

#include "random/Simplex.hpp"
#include "random/WorleyNoise.hpp"
#include <util/img/Bitmap.hpp>

#include <math/Vector2.hpp>
#include <math/Vector3.hpp>

#include <core/lib/FlatMap.hpp>
#include <core/lib/SortedArray.hpp>
#include <core/lib/Range.hpp>

#include <Types.hpp>

#include <map>
#include <utility>
#include <random>

namespace hyperion {

using namespace v2;

using Seed = UInt32;

enum NoiseGenerationType
{
    BASIC_NOISE,
    SIMPLEX_NOISE,
    WORLEY_NOISE
};

class NoiseGenerator
{
    friend class NoiseFactory;
public:
    NoiseGenerator(NoiseGenerationType type, Seed seed);
    virtual ~NoiseGenerator() = default;

    Seed GetSeed() const { return m_seed; }

    virtual double GetNoise(double x, double z) const = 0;
    virtual double GetNoise(double x, double y, double z) const
        { return GetNoise(x, y); }

    double GetNoise(const Vector2 &xy) const
        { return GetNoise(xy.x, xy.y); }

    double GetNoise(const Vector3 &xyz) const
        { return GetNoise(xyz.x, xyz.y, xyz.z); }

    Bitmap<1> CreateBitmap(UInt width, UInt height, Float scale = 1.0f);

protected:
    Seed m_seed;

private:
    NoiseGenerationType m_type;
};

template <class T>
class BasicNoiseGenerator
{
public:
    BasicNoiseGenerator(Seed seed, const Range<T> &range)
        : m_seed(seed),
          m_range(range),
          m_mt { m_random_device() },
          m_distribution { range.GetStart(), range.GetEnd() }
    {
    }

    virtual ~BasicNoiseGenerator() = default;

    virtual T Next() const
        { return m_distribution(); }

protected:
    Seed m_seed;

    Range<T> m_range;

    std::random_device m_random_device;
    std::mt19937 m_mt;
    std::uniform_real_distribution<T> m_distribution;
};

class SimplexNoiseGenerator : public NoiseGenerator
{
public:
    SimplexNoiseGenerator(Seed seed);
    virtual ~SimplexNoiseGenerator();

    virtual double GetNoise(double x, double z) const override;
    virtual double GetNoise(double x, double y, double z) const override;

private:
    SimplexNoiseData m_simplex_noise;
};

class WorleyNoiseGenerator : public NoiseGenerator
{
public:
    WorleyNoiseGenerator(Seed seed);
    virtual ~WorleyNoiseGenerator();

    virtual double GetNoise(double x, double z) const override;
    virtual double GetNoise(double x, double y, double z) const override;

private:
    WorleyNoise *m_worley_noise;
};

class NoiseCombinator
{
public:
    enum class Mode {
        ADDITIVE,
        MULTIPLICATIVE
    };

    struct NoiseGeneratorInstance
    {
        Mode                        mode;
        UniquePtr<NoiseGenerator>   generator;
        Float                       multiplier; // amount to multiply a result by
        Float                       bias; // amount to add to a result (pre-mult)
        Vec3f                       scaling; // coord scaling
    };

    NoiseCombinator()
        : m_seed(0)
    {
    }

    NoiseCombinator(Seed seed)
        : m_seed(seed)
    {
    }

    NoiseCombinator(const NoiseCombinator &other)                   = delete;
    NoiseCombinator &operator=(const NoiseCombinator &other)        = delete;
    NoiseCombinator(NoiseCombinator &&other) noexcept               = default;
    NoiseCombinator &operator=(NoiseCombinator &&other) noexcept    = default;
    ~NoiseCombinator()                                              = default;

    Seed GetSeed() const
        { return m_seed; }

    void SetSeed(Seed seed)
        { m_seed = seed; }

    template <class NoiseGeneratorType>
    NoiseCombinator &Use(
        Int priority,
        Mode mode = Mode::ADDITIVE,
        Float multiplier = 1.0f,
        Float bias = 0.0f,
        const Vector3 &scaling = Vector3::One()
    )
    {
        static_assert(std::is_base_of_v<NoiseGenerator, NoiseGeneratorType>,
            "Must be a derived class of NoiseGenerator!");

        m_generators.Insert({
            priority,
            NoiseGeneratorInstance {
                .mode       = mode,
                .generator  = UniquePtr<NoiseGeneratorType>::Construct(m_seed),
                .multiplier = multiplier,
                .bias       = bias,
                .scaling    = scaling
            }
        });

        return *this;
    }

    Float GetNoise(const Vector2 &xy) const
    {
        Float result = 0.0f;

        bool first = true;

        for (auto &it : m_generators) {
            ApplyNoiseValue(
                it.second.mode,
                (it.second.generator->GetNoise(xy * Vector2(it.second.scaling.x, it.second.scaling.y)) + it.second.bias) * it.second.multiplier,
                result,
                first
            );
        }

        return result;
    }

    Float GetNoise(const Vector3 &xyz) const
    {
        Float result = 0.0f;

        bool first = true;

        for (auto &it : m_generators) {
            ApplyNoiseValue(
                it.second.mode,
                (it.second.generator->GetNoise(xyz * Vector3(it.second.scaling)) + it.second.bias) * it.second.multiplier,
                result,
                first
            );
        }

        return result;
    }

protected:
    static void ApplyNoiseValue(Mode mode, Float noise_value, Float &final_result, bool &first)
    {
        switch (mode) {
        case Mode::ADDITIVE:
            final_result += noise_value;

            break;
        case Mode::MULTIPLICATIVE:
            if (first) {
                final_result = 1.0f;
            }

            final_result *= noise_value;

            break;
        }

        first = false;
    }

    Seed m_seed;
    SortedArray<KeyValuePair<Int, NoiseGeneratorInstance>> m_generators;
};


struct NoiseGeneratorRefCounter
{
    NoiseGenerator *noise;
    SizeType uses;
};

class NoiseFactory
{
public:
    static NoiseFactory *instance;
    static NoiseFactory *GetInstance();

    NoiseFactory() = default;
    ~NoiseFactory() = default;

    NoiseGenerator *Capture(NoiseGenerationType type, Seed seed);
    void Release(NoiseGenerator *noise);
    void Release(NoiseGenerationType type, Seed seed);

private:
    std::map<
        std::pair<NoiseGenerationType, Seed>,
        NoiseGeneratorRefCounter
    > m_noise_generators;
};

} // namespace hyperion

#endif
