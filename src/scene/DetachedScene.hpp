/* Copyright (c) 2025 No Tomorrow Games. All rights reserved. */

#pragma once

#include <core/Defines.hpp>

namespace Hyperion {

class Scene;

namespace threading {
class ThreadId;
} // namespace threading

using threading::ThreadId;

HYP_API extern Scene* GetDetachedSceneForCurrentThread();
HYP_API extern Scene* GetDetachedSceneForThread(const ThreadId& threadId);

} // namespace Hyperion
