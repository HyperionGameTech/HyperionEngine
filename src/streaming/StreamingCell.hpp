/* Copyright (c) 2024-2025 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_STREAMING_CELL_HPP
#define HYPERION_STREAMING_CELL_HPP

#include <core/threading/Task.hpp>
#include <core/threading/Mutex.hpp>

#include <core/object/HypObject.hpp>

#include <core/math/Vector2.hpp>

#include <scene/Entity.hpp>
#include <scene/Node.hpp>

#include <streaming/Streamable.hpp>

#include <GameCounter.hpp>
#include <HashCode.hpp>

namespace hyperion {

class WorldGrid;
class Scene;

HYP_ENUM()
enum class StreamingCellState : uint32
{
    UNLOADED,
    UNLOADING,
    WAITING,
    LOADED
};

HYP_STRUCT()
struct StreamingCellNeighbor
{
    HYP_FIELD(Serialize, Property = "Coord")
    Vec2i coord;

    HYP_FORCE_INLINE Vec2f GetCenter() const
    {
        return Vec2f(coord) - 0.5f;
    }
};

HYP_STRUCT(Size = 128)
struct StreamingCellInfo
{
    HYP_FIELD(Serialize, Property = "Extent")
    Vec3u extent;

    HYP_FIELD(Serialize, Property = "Coord")
    Vec2i coord;

    HYP_FIELD(Serialize, Property = "Scale")
    Vec3f scale = Vec3f::One();

    HYP_FIELD(Serialize, Property = "State")
    StreamingCellState state = StreamingCellState::UNLOADED;

    HYP_FIELD(Serialize, Property = "Neighbors")
    FixedArray<StreamingCellNeighbor, 8> neighbors;

    HYP_FORCE_INLINE HashCode GetHashCode() const
    {
        HashCode hash_code;

        hash_code.Add(extent);
        hash_code.Add(coord);
        hash_code.Add(scale);
        hash_code.Add(state);

        return hash_code;
    }
};

HYP_CLASS()
class HYP_API StreamingCell : public StreamableBase
{
    HYP_OBJECT_BODY(StreamingCell);

public:
    StreamingCell() = default;
    StreamingCell(WorldGrid* world_grid, const StreamingCellInfo& cell_info);
    virtual ~StreamingCell() override;

    HYP_METHOD()
    HYP_FORCE_INLINE WorldGrid* GetWorldGrid() const
    {
        return m_world_grid;
    }

    HYP_METHOD()
    HYP_FORCE_INLINE const StreamingCellInfo& GetPatchInfo() const
    {
        return m_cell_info;
    }

    HYP_METHOD()
    HYP_FORCE_INLINE StreamingCellState GetState() const
    {
        return m_cell_info.state;
    }

    HYP_METHOD()
    HYP_FORCE_INLINE void SetState(StreamingCellState state)
    {
        m_cell_info.state = state;
    }

    HYP_METHOD(Scriptable)
    void Update(float delta);

protected:
    HYP_METHOD()
    virtual BoundingBox GetBoundingBox_Impl() const override;

    HYP_METHOD()
    virtual void Update_Impl(float delta)
    {
    }

    WorldGrid* m_world_grid;
    StreamingCellInfo m_cell_info;
};

} // namespace hyperion

#endif
