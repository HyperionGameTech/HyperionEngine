/* Copyright (c) 2025 No Tomorrow Games. All rights reserved. */

#pragma once

#include <core/Defines.hpp>
#include <core/Constants.hpp>

namespace hyperion {

namespace memory {
class Pool;
class LinearPool;
} // namespace memory

using memory::LinearPool;
using memory::Pool;

HYP_API extern Pool* g_renderPool;
HYP_API extern LinearPool* g_framePools[NumMultiBuffers];

} // namespace hyperion
