#pragma once

#include <Core/FxDefines.hpp>

// If we are on another platform and wish to emulate AVX, use the SIMDE wrapper headers
#ifdef FX_USE_SIMDE
#include <simde/avx2.h>
#include <simde/fma.h>
#include <simde/sse4.2.h>
#else
// Sanity check to make sure this is an AVX build
#ifdef FX_USE_AVX
#include <immintrin.h>
#else
#warning "FxSSE.hpp should not be included when producing a non-SSE build!"
#endif
#endif
