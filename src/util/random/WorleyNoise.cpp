/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include "WorleyNoise.hpp"

#include <algorithm>
#include <cmath>

namespace hyperion {
WorleyNoise::WorleyNoise(int seed)
    : m_seed(seed)
{
}

double WorleyNoise::Noise(double x, double y, double z)
{
    Vector3 input_point(x, y, z);
    Vector3 random_diff;
    Vector3 feature_point;

    size_t last_random = 0;
    size_t num_feature_points = 0;

    long long int cubex = 0, cubey = 0, cubez = 0;
    long long int eval_cubex = std::floor((int)x);
    long long int eval_cubey = std::floor((int)y);
    long long int eval_cubez = std::floor((int)z);

    std::vector<double> distance_array { 6666, 6666, 6666 };

    for (int i = -1; i < 2; i++) {
        for (int j = -1; j < 2; j++) {
            for (int k = -1; k < 2; k++) {
                cubex = eval_cubex + i;
                cubey = eval_cubey + j;
                cubez = eval_cubez + k;

                last_random = LCGRandom(WorleyHash(cubex + m_seed, cubey, cubez));
                num_feature_points = ProbLookup(last_random);

                for (unsigned int l = 0; l < num_feature_points; l++) {
                    last_random = LCGRandom(last_random);
                    random_diff.x = double(last_random) / 0x100000000;

                    last_random = LCGRandom(last_random);
                    random_diff.y = double(last_random) / 0x100000000;

                    last_random = LCGRandom(last_random);
                    random_diff.z = double(last_random) / 0x100000000;

                    feature_point = Vector3(random_diff.x + cubex, random_diff.y + cubey, random_diff.z + cubez);

                    Insert(distance_array, EuclidianDistance(input_point, feature_point));
                }
            }
        }
    }

    return std::min(std::max(CombinerFunc1(distance_array.data()), 0.0), 1.0);
}

double WorleyNoise::CombinerFunc1(double *data)
{
    return data[0];
}

double WorleyNoise::CombinerFunc2(double *data)
{
    return data[1] - data[0];
}

double WorleyNoise::CombinerFunc3(double *data)
{
    return data[2] - data[0];
}

double WorleyNoise::EuclidianDistance(const Vector3 &v1, const Vector3 &v2)
{
    return v1.DistanceSquared(v2);
}
double WorleyNoise::ManhattanDistance(const Vector3 &v1, const Vector3 &v2)
{
    return abs(v1.x - v2.x) + abs(v1.y - v2.y) + abs(v1.z - v2.z);
}

double WorleyNoise::ChebyshevDistance(const Vector3 &v1, const Vector3 &v2)
{
    Vector3 d = v1 - v2;
    return std::max(std::max(std::abs(d.x), std::abs(d.y)), std::abs(d.z));
}

unsigned char WorleyNoise::ProbLookup(unsigned long long value)
{
    if (value < 393325350) return 1;
    if (value < 1022645910) return 2;
    if (value < 1861739990) return 3;
    if (value < 2700834071) return 4;
    if (value < 3372109335) return 5;
    if (value < 3819626178) return 6;
    if (value < 4075350088) return 7;
    if (value < 4203212043) return 8;
    return 9;
}

void WorleyNoise::Insert(std::vector<double> &data, double value)
{
    double temp;
    for (long i = data.size() - 1; i >= 0; i--) {
        if (value > data[i]) {
            break;
        }
        temp = data[i];
        data[i] = value;
        if (i + 1 < data.size()) {
            data[i + 1] = temp;
        }
    }
}
} // namespace hyperion
