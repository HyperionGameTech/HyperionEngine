/* Copyright (c) 2025 No Tomorrow Games. All rights reserved. */

#pragma once

#include <engine/EngineMemory.hpp>

namespace Hyperion {

namespace RenderApi {
HYP_API extern uint32 GetRingIndex();
} // namespace RenderApi

HYP_API extern Pool* GetCurrentFramePool();

} // namespace Hyperion
