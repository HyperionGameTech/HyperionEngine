#include <HyperionPch.hpp>

#include <util/random/WorleyNoise.hpp>

#include <algorithm>
#include <cmath>

namespace Hyperion {
WorleyNoise::WorleyNoise(int seed)
    : m_seed(seed)
{
}

double WorleyNoise::Noise(double x, double y, double z)
{
    Vector3 inputPoint(x, y, z);
    Vector3 randomDiff;
    Vector3 featurePoint;

    size_t lastRandom = 0;
    size_t numFeaturePoints = 0;

    long long int cubex = 0, cubey = 0, cubez = 0;
    long long int evalCubex = std::floor((int)x);
    long long int evalCubey = std::floor((int)y);
    long long int evalCubez = std::floor((int)z);

    std::vector<double> distanceArray { 6666, 6666, 6666 };

    for (int i = -1; i < 2; i++)
    {
        for (int j = -1; j < 2; j++)
        {
            for (int k = -1; k < 2; k++)
            {
                cubex = evalCubex + i;
                cubey = evalCubey + j;
                cubez = evalCubez + k;

                lastRandom = LCGRandom(WorleyHash(cubex + m_seed, cubey, cubez));
                numFeaturePoints = ProbLookup(lastRandom);

                for (unsigned int l = 0; l < numFeaturePoints; l++)
                {
                    lastRandom = LCGRandom(lastRandom);
                    randomDiff.x = double(lastRandom) / 0x100000000;

                    lastRandom = LCGRandom(lastRandom);
                    randomDiff.y = double(lastRandom) / 0x100000000;

                    lastRandom = LCGRandom(lastRandom);
                    randomDiff.z = double(lastRandom) / 0x100000000;

                    featurePoint = Vector3(randomDiff.x + cubex, randomDiff.y + cubey, randomDiff.z + cubez);

                    Insert(distanceArray, EuclidianDistance(inputPoint, featurePoint));
                }
            }
        }
    }

    return std::min(std::max(CombinerFunc1(distanceArray.data()), 0.0), 1.0);
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
