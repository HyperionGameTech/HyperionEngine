/* Copyright (c) 2026 Andrew J. MacDonald. All rights reserved. */

#pragma once

#include <Core/Defines.hpp>

namespace Hyperion {

class Scene;

namespace threading {
class ThreadId;
} // namespace threading

using threading::ThreadId;

void DestroyDetachedScenes();

Scene* GetDetachedSceneForCurrentThread();
Scene* GetDetachedSceneForThread(const ThreadId& threadId);

} // namespace Hyperion
