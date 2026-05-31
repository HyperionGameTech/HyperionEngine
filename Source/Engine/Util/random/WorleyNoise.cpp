#include <HyperionPch.hpp>

#include <Util/random/WorleyNoise.hpp>

#include <algorithm>
#include <cmath>

namespace Hyperion {

static float GetRandom(int seed, Vec3i pt)
{
    int n = pt.x + pt.y * 57 + pt.z * 131 + seed * 1337;
    n = (n << 13) ^ n;
    return (1.0f - ((n * (n * n * 15731 + 789221) + 1376312589) & 0x7fffffff) / 1073741824.0f);
}

static Vec3f GetFeaturePoint(int seed, Vec3i pt)
{
    float r1 = GetRandom(seed, pt);
    float r2 = GetRandom(seed, { pt.x + 23, pt.y - 105, pt.z + 401 });
    float r3 = GetRandom(seed, { pt.x - 11, pt.y + 333, pt.z - 99 });

    return { (float)pt.x + r1, (float)pt.y + r2, (float)pt.z + r3 };
}

WorleyNoise::WorleyNoise(int seed)
    : m_seed(seed)
{
}

double WorleyNoise::Noise(double x, double y, double z)
{
    int ix = std::floor(x);
    int iy = std::floor(y);
    int iz = std::floor(z);

    float minDistSqr = 1000.0f;

    for (int dz = -1; dz <= 1; ++dz)
    {
        for (int dy = -1; dy <= 1; ++dy)
        {
            for (int dx = -1; dx <= 1; ++dx)
            {
                const int nx = ix + dx;
                const int ny = iy + dy;
                const int nz = iz + dz;

                const Vec3 point = GetFeaturePoint(m_seed, { nx, ny, nz });

                const float distSqr = Vec3f(float(x), float(y), float(z)).DistanceSquared(point);

                if (distSqr < minDistSqr)
                {
                    minDistSqr = distSqr;
                }
            }
        }
    }

    return std::sqrt(minDistSqr);
}

double WorleyNoise::CombinerFunc1(double* data)
{
    return data[0];
}

double WorleyNoise::CombinerFunc2(double* data)
{
    return data[1] - data[0];
}

double WorleyNoise::CombinerFunc3(double* data)
{
    return data[2] - data[0];
}

double WorleyNoise::EuclidianDistance(const Vector3& v1, const Vector3& v2)
{
    return v1.DistanceSquared(v2);
}

double WorleyNoise::ManhattanDistance(const Vector3& v1, const Vector3& v2)
{
    return abs(v1.x - v2.x) + abs(v1.y - v2.y) + abs(v1.z - v2.z);
}

double WorleyNoise::ChebyshevDistance(const Vector3& v1, const Vector3& v2)
{
    Vector3 d = v1 - v2;
    return std::max(std::max(std::abs(d.x), std::abs(d.y)), std::abs(d.z));
}

unsigned char WorleyNoise::ProbLookup(unsigned long long value)
{
    if (value < 393325350)
        return 1;
    if (value < 1022645910)
        return 2;
    if (value < 1861739990)
        return 3;
    if (value < 2700834071)
        return 4;
    if (value < 3372109335)
        return 5;
    if (value < 3819626178)
        return 6;
    if (value < 4075350088)
        return 7;
    if (value < 4203212043)
        return 8;
    return 9;
}

void WorleyNoise::Insert(std::vector<double>& data, double value)
{
    double temp;
    for (long i = data.size() - 1; i >= 0; i--)
    {
        if (value > data[i])
        {
            break;
        }
        temp = data[i];
        data[i] = value;
        if (i + 1 < data.size())
        {
            data[i + 1] = temp;
        }
    }
}
} // namespace Hyperion
