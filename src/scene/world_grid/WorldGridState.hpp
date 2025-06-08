/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_WORLD_GRID_STATE_HPP
#define HYPERION_WORLD_GRID_STATE_HPP

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

#include <GameCounter.hpp>
#include <HashCode.hpp>

namespace hyperion {

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
    AtomicVar<bool> has_updates;
};

struct WorldGridState
{
    FlatMap<Vec2i, Task<void>> patch_generation_tasks;
    Queue<Handle<StreamingCell>> patch_generation_queue_owned;
    WorldGridPatchGenerationQueue patch_generation_queue_shared;

    Queue<StreamingCellUpdate> patch_update_queue;
    AtomicVar<uint32> patch_update_queue_size { 0 };
    mutable Mutex patch_update_queue_mutex;

    FlatMap<Vec2i, Handle<StreamingCell>> patches;
    mutable Mutex patches_mutex;

    // Keep track of the last desired patches to avoid unnecessary comparison and locking
    HashCode::ValueType previous_desired_patch_coords_hash = 0;

    void PushUpdate(StreamingCellUpdate&& update);
};

} // namespace hyperion

#endif
