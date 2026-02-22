/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <streaming/StreamingCell.hpp>

#include <core/containers/FlatMap.hpp>
#include <core/containers/FlatSet.hpp>
#include <core/containers/Queue.hpp>

#include <core/memory/UniquePtr.hpp>

#include <core/threading/Task.hpp>
#include <core/threading/Mutex.hpp>

#include <scene/Entity.hpp>
#include <scene/Node.hpp>

#include <core/math/Vector2.hpp>

#include <core/HashCode.hpp>

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
    FlatMap<Vec2i, Task<void>> patchGenerationTasks;

    Queue<StreamingCellUpdate> patchUpdateQueue;
    AtomicVar<uint32> patchUpdateQueueSize { 0 };
    mutable Mutex patchUpdateQueueMutex;

    FlatMap<Vec2i, Handle<StreamingCell>> patches;
    mutable Mutex patchesMutex;

    // Keep track of the last desired patches to avoid unnecessary comparison and locking
    HashCode::ValueType previousDesiredPatchCoordsHash = 0;

    void PushUpdate(StreamingCellUpdate&& update);
};

} // namespace Hyperion
