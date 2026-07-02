/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#pragma once

#include <Streaming/StreamingCell.hpp>

#include <Core/Containers/Map.hpp>
#include <Core/Containers/Queue.hpp>

#include <Core/Memory/UniquePtr.hpp>

#include <Core/Threading/Task.hpp>
#include <Core/Threading/Mutex.hpp>

#include <Scene/Entity.hpp>
#include <Scene/Node.hpp>

#include <Core/Math/Vector2.hpp>

#include <Core/HashCode.hpp>

namespace Hyperion {

class Scene;

enum class StreamingCellState : uint32;

struct StreamingCellUpdate
{
    Vec2i coord;
    StreamingCellState state;
};

struct WorldGridPatchGenerationQueue
{
    Mutex mutex;
    Queue<Handle<StreamingCell>> queue;
    AtomicVar<bool> hasUpdates;
};

struct WorldGridState
{
    Map<Vec2i, Task<void>> patchGenerationTasks;

    Queue<StreamingCellUpdate> patchUpdateQueue;
    AtomicVar<uint32> patchUpdateQueueSize { 0 };
    mutable Mutex patchUpdateQueueMutex;

    Map<Vec2i, Handle<StreamingCell>> patches;
    mutable Mutex patchesMutex;

    // Keep track of the last desired patches to avoid unnecessary comparison and locking
    HashCode::ValueType previousDesiredPatchCoordsHash = 0;

    void PushUpdate(StreamingCellUpdate&& update);
};

} // namespace Hyperion
