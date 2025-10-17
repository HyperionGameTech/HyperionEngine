/* Copyright (c) 2025 No Tomorrow Games. All rights reserved. */

#pragma once

#include <streaming/StreamingCell.hpp>

#include <core/reflection/Handle.hpp>
#include <core/Defines.hpp>

#include <core/containers/Array.hpp>
#include <core/containers/HashSet.hpp>

#include <core/threading/AtomicVar.hpp>

#include <core/utilities/Result.hpp>

#include <core/math/Vector2.hpp>

namespace hyperion {

struct StreamingCellRuntimeInfo
{
    Vec2i coord;
    StreamingCellState state; // used internally on streaming thread and worker threads - not game thread safe.
    AtomicVar<bool> isLocked;
    Handle<StreamingCell> cell;

    StreamingCellRuntimeInfo()
        : coord(Vec2i::Zero()),
          cell(),
          state(StreamingCellState::INVALID),
          isLocked(false)
    {
    }

    StreamingCellRuntimeInfo(const Vec2i& coord, const Handle<StreamingCell>& cell, StreamingCellState state, bool isLocked = false)
        : coord(coord),
          cell(cell),
          state(state),
          isLocked(isLocked)
    {
    }

    StreamingCellRuntimeInfo(const StreamingCellRuntimeInfo& other) = delete;
    StreamingCellRuntimeInfo& operator=(const StreamingCellRuntimeInfo& other) = delete;

    StreamingCellRuntimeInfo(StreamingCellRuntimeInfo&& other) noexcept
        : coord(std::move(other.coord)),
          cell(std::move(other.cell)),
          state(other.state),
          isLocked(other.isLocked.Exchange(false, MemoryOrder::ACQUIRE_RELEASE))
    {
        other.state = StreamingCellState::INVALID;
    }

    StreamingCellRuntimeInfo& operator=(StreamingCellRuntimeInfo&& other) noexcept
    {
        if (this != &other)
        {
            coord = std::move(other.coord);
            cell = std::move(other.cell);
            state = other.state;
            isLocked.Exchange(other.isLocked.Exchange(false, MemoryOrder::ACQUIRE_RELEASE), MemoryOrder::RELEASE);

            other.state = StreamingCellState::INVALID;
        }

        return *this;
    }

    ~StreamingCellRuntimeInfo() = default;
};

class StreamingCellCollection final : HashSet<StreamingCellRuntimeInfo, &StreamingCellRuntimeInfo::coord, DynamicNodeAllocator>
{
public:
    using Iterator = typename HashSet::Iterator;
    using ConstIterator = typename HashSet::ConstIterator;

    StreamingCellCollection()
    {
    }

    StreamingCellCollection(const StreamingCellCollection& other) = default;
    StreamingCellCollection(StreamingCellCollection&& other) = default;

    StreamingCellCollection& operator=(const StreamingCellCollection& other) = default;
    StreamingCellCollection& operator=(StreamingCellCollection&& other) = default;

    using HashSet::Any;
    using HashSet::Empty;
    using HashSet::Size;

    bool AddCell(const Handle<StreamingCell>& cell, StreamingCellState initialState, bool lock = false)
    {
        if (!cell.IsValid())
        {
            return false;
        }

        auto it = HashSet::Find(cell->GetPatchInfo().coord);
        if (it != HashSet::End())
        {
            // Cell already exists
            return false;
        }

        auto insertResult = HashSet::Emplace(cell->GetPatchInfo().coord, cell, initialState, lock);
        AssertDebug(insertResult.second);

        return true;
    }

    bool RemoveCell(const Vec2i& coord)
    {
        auto it = HashSet::Find(coord);
        if (it != HashSet::End())
        {
            HashSet::Erase(it);

            return true;
        }

        return false;
    }

    Handle<StreamingCell> GetCell(const Vec2i& coord) const
    {
        auto it = HashSet::Find(coord);
        if (it != HashSet::End())
        {
            return it->cell;
        }

        return Handle<StreamingCell>();
    }

    bool HasCell(const Vec2i& coord) const
    {
        return HashSet::Find(coord) != HashSet::End();
    }

    bool SetCellLockState(const Vec2i& coord, bool lock)
    {
        auto it = HashSet::Find(coord);
        if (it != HashSet::End())
        {
            if (it->isLocked.Exchange(lock, MemoryOrder::ACQUIRE_RELEASE) == lock)
            {
                return false;
            }

            return true;
        }

        return false;
    }

    bool IsCellLocked(const Vec2i& coord) const
    {
        auto it = HashSet::Find(coord);
        if (it != HashSet::End())
        {
            return it->isLocked.Get(MemoryOrder::ACQUIRE);
        }

        return false;
    }

    bool UpdateCellState(const Vec2i& coord, StreamingCellState newState)
    {
        auto it = HashSet::Find(coord);
        if (it != HashSet::End())
        {
            it->state = newState;
            return true;
        }

        return false;
    }

    StreamingCellState GetCellState(const Vec2i& coord) const
    {
        auto it = HashSet::Find(coord);

        if (it != HashSet::End())
        {
            return it->state;
        }

        return StreamingCellState::INVALID; // Default state if not found
    }

    void Clear()
    {
        HashSet::Clear();
    }

    HYP_DEF_STL_BEGIN_END(HashSet::Begin(), HashSet::End());
};

} // namespace hyperion
