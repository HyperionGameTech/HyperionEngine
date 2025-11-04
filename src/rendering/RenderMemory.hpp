/* Copyright (c) 2025 No Tomorrow Games. All rights reserved. */

#pragma once

#include <core/Defines.hpp>
#include <core/Constants.hpp>

#include <core/memory/allocator/Allocator.hpp>

#include <engine/EngineMemory.hpp>

namespace hyperion {

namespace RenderApi {
HYP_API extern uint32 GetRingIndex();
} // namespace RenderApi

HYP_API extern Pool* GetCurrentFramePool();

} // namespace hyperion
