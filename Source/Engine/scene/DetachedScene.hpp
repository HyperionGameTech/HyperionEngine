/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

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
