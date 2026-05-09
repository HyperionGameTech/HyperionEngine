/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <streaming/StreamingCell.hpp>

#include <Core/reflection/Handle.hpp>
#include <Core/Defines.hpp>

#include <Core/containers/Array.hpp>
#include <Core/containers/Set.hpp>

#include <Core/threading/AtomicVar.hpp>

#include <Core/utilities/Result.hpp>

#include <Core/math/Vector2.hpp>

namespace Hyperion {

struct StreamingCellRuntimeInfo
{
    Vec2i coord;
    StreamingCellState state; // used internally on streaming thread and worker threads - not sim thread safe.
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

template <class AllocatorType>
class StreamingCellCollection final : THashTable<StreamingCellRuntimeInfo, &StreamingCellRuntimeInfo::coord, AllocatorType>
{
public:
    using Base = THashTable<StreamingCellRuntimeInfo, &StreamingCellRuntimeInfo::coord, AllocatorType>;

    using Iterator = typename Base::Iterator;
    using ConstIterator = typename Base::ConstIterator;

    StreamingCellCollection()
    {
    }

    StreamingCellCollection(const StreamingCellCollection& other) = default;
    StreamingCellCollection(StreamingCellCollection&& other) = default;

    StreamingCellCollection& operator=(const StreamingCellCollection& other) = default;
    StreamingCellCollection& operator=(StreamingCellCollection&& other) = default;

    using Base::Any;
    using Base::Empty;
    using Base::Size;

    bool AddCell(const Handle<StreamingCell>& cell, StreamingCellState initialState, bool lock = false)
    {
        if (!cell.IsValid())
        {
            return false;
        }

        auto it = Base::Find(cell->GetPatchInfo().coord);
        if (it != Base::End())
        {
            // Cell already exists
            return false;
        }

        auto insertResult = Base::Emplace(cell->GetPatchInfo().coord, cell, initialState, lock);
        AssertDebug(insertResult.second);

        return true;
    }

    bool RemoveCell(const Vec2i& coord)
    {
        auto it = Base::Find(coord);
        if (it != Base::End())
        {
            Base::Erase(it);

            return true;
        }

        return false;
    }

    Handle<StreamingCell> GetCell(const Vec2i& coord) const
    {
        auto it = Base::Find(coord);
        if (it != Base::End())
        {
            return it->cell;
        }

        return Handle<StreamingCell>();
    }

    bool HasCell(const Vec2i& coord) const
    {
        return Base::Find(coord) != Base::End();
    }

    bool SetCellLockState(const Vec2i& coord, bool lock)
    {
        auto it = Base::Find(coord);
        if (it != Base::End())
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
        auto it = Base::Find(coord);
        if (it != Base::End())
        {
            return it->isLocked.Get(MemoryOrder::ACQUIRE);
        }

        return false;
    }

    bool UpdateCellState(const Vec2i& coord, StreamingCellState newState)
    {
        auto it = Base::Find(coord);
        if (it != Base::End())
        {
            it->state = newState;
            return true;
        }

        return false;
    }

    StreamingCellState GetCellState(const Vec2i& coord) const
    {
        auto it = Base::Find(coord);

        if (it != Base::End())
        {
            return it->state;
        }

        return StreamingCellState::INVALID; // Default state if not found
    }

    void Clear()
    {
        Base::Clear();
    }

    HYP_DEF_STL_BEGIN_END(Base::Begin(), Base::End());
};

} // namespace Hyperion
