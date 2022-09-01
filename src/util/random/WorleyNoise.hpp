#ifndef WORLEY_NOISE_H
#define WORLEY_NOISE_H

#include "../../math/Vector3.hpp"

#include <Types.hpp>

#include <vector>

namespace hyperion {
class WorleyNoise {
    static constexpr UInt32 OFFSET_BASIS = 2166136261u;
    static constexpr UInt32 FNV_PRIME    = 16777619u;

public:
    WorleyNoise(int seed);

    double Noise(double x, double y, double z);

private:
    int m_seed;

    double CombinerFunc1(double *data);
    double CombinerFunc2(double *data);
    double CombinerFunc3(double *data);

    double EuclidianDistance(const Vector3 &v1, const Vector3 &v2);
    double ManhattanDistance(const Vector3 &v1, const Vector3 &v2);
    double ChebyshevDistance(const Vector3 &v1, const Vector3 &v2);

    static unsigned char ProbLookup(unsigned long long value);

    void Insert(std::vector<double> &data, double value);

    size_t LCGRandom(size_t last) const
    {
        return (1103515245ULL * last + 12345ULL) % 0x100000000ULL;
    }

    size_t WorleyHash(size_t i, size_t j, size_t k) const
    {
        return ((((((OFFSET_BASIS ^ i) * FNV_PRIME) ^ j) * FNV_PRIME) ^ k) * FNV_PRIME);
    }
};
} // namespace hyperion

#endif
