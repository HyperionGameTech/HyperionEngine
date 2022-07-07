#ifndef SIMPLEX_H
#define SIMPLEX_H

#include "OpenSimplexNoise.hpp"

#define OSN_OCTAVE_COUNT 8

struct osn_context;

namespace hyperion {
struct SimplexNoiseData {
    osn_context *octaves[OSN_OCTAVE_COUNT];
    double frequencies[OSN_OCTAVE_COUNT];
    double amplitudes[OSN_OCTAVE_COUNT];
};
} // namespace hyperion

#endif
