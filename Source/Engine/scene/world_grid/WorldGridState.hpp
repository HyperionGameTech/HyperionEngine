/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <streaming/StreamingCell.hpp>

#include <Core/containers/FlatMap.hpp>
#include <Core/containers/FlatSet.hpp>
#include <Core/containers/Queue.hpp>

#include <Core/memory/UniquePtr.hpp>

#include <Core/threading/Task.hpp>
#include <Core/threading/Mutex.hpp>

#include <scene/Entity.hpp>
#include <scene/Node.hpp>

#include <Core/math/Vector2.hpp>

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
