/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <OpenSimplexNoise.hpp>

#define OSN_OCTAVE_COUNT 8

struct osnContext;

namespace Hyperion {
struct SimplexNoiseData
{
    osn_context* octaves[OSN_OCTAVE_COUNT];
    double frequencies[OSN_OCTAVE_COUNT];
    double amplitudes[OSN_OCTAVE_COUNT];
};
} // namespace Hyperion
