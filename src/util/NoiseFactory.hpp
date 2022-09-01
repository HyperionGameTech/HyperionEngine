#ifndef NOISE_FACTORY_H
#define NOISE_FACTORY_H

#include "random/Simplex.hpp"
#include "random/WorleyNoise.hpp"

#include <math/Vector2.hpp>
#include <math/Vector3.hpp>
#include <math/Vector4.hpp>

#include <core/lib/FlatMap.hpp>
#include <core/lib/SortedArray.hpp>

#include <Types.hpp>

#include <vector>
#include <map>
#include <utility>

namespace hyperion {

using Seed = UInt32;

enum NoiseGenerationType {
    SIMPLEX_NOISE,
    WORLEY_NOISE
};

class NoiseGenerator {
    friend class NoiseFactory;
public:
    NoiseGenerator(NoiseGenerationType type, Seed seed);
    virtual ~NoiseGenerator() = default;

    Seed GetSeed() const { return m_seed; }

    virtual double GetNoise(double x, double z) const = 0;
    virtual double GetNoise(double x, double y, double z) const { return GetNoise(x, y); }

    double GetNoise(const Vector2 &xy) const { return GetNoise(xy.x, xy.y); }
    double GetNoise(const Vector3 &xyz) const { return GetNoise(xyz.x, xyz.y, xyz.z); }

protected:
    Seed m_seed;

private:
    NoiseGenerationType m_type;
};

class SimplexNoiseGenerator : public NoiseGenerator {
public:
    SimplexNoiseGenerator(Seed seed);
    virtual ~SimplexNoiseGenerator();

    virtual double GetNoise(double x, double z) const override;
    virtual double GetNoise(double x, double y, double z) const override;

private:
    SimplexNoiseData m_simplex_noise;
};

class WorleyNoiseGenerator : public NoiseGenerator {
public:
    WorleyNoiseGenerator(Seed seed);
    virtual ~WorleyNoiseGenerator();

    virtual double GetNoise(double x, double z) const override;
    virtual double GetNoise(double x, double y, double z) const override;

private:
    WorleyNoise *m_worley_noise;
};

class NoiseCombinator {
public:
    enum class Mode {
        ADDITIVE,
        MULTIPLICATIVE
    };

    struct NoiseGeneratorInstance
    {
        Mode mode;
        std::unique_ptr<NoiseGenerator> generator;
        Float multiplier; // amount to multiply a result by
        Float bias; // amount to add to a result (pre-mult)
        Vector4 scaling; // coord scaling
    };

    NoiseCombinator(Seed seed)
        : m_seed(seed)
    {
    }

    ~NoiseCombinator() = default;

    template <class NoiseGeneratorType>
    NoiseCombinator &Use(
        UInt priority,
        Mode mode = Mode::ADDITIVE,
        Float multiplier = 1.0f,
        Float bias = 0.0f,
        const Vector4 &scaling = Vector4::One()
    )
    {
        static_assert(std::is_base_of_v<NoiseGenerator, NoiseGeneratorType>,
            "Must be a derived class of NoiseGenerator!");

        m_generators.Insert({
            priority,
            NoiseGeneratorInstance {
                .mode = mode,
                .generator = std::make_unique<NoiseGeneratorType>(m_seed),
                .multiplier = multiplier,
                .bias = bias,
                .scaling = scaling
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
                (it.second.generator->GetNoise(xy * Vector2(it.second.scaling)) + it.second.bias) * it.second.multiplier,
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


struct NoiseGeneratorRefCounter {
    NoiseGenerator *noise;
    size_t          uses;
};

class NoiseFactory {
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
