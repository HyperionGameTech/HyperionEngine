/* Copyright (c) 2025 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_STREAMING_CELL_COLLECTION_HPP
#define HYPERION_STREAMING_CELL_COLLECTION_HPP

#include <streaming/StreamingCell.hpp>

#include <core/Handle.hpp>
#include <core/Defines.hpp>

#include <core/containers/Array.hpp>
#include <core/containers/FlatSet.hpp>

#include <core/utilities/Result.hpp>

#include <core/math/Vector2.hpp>

namespace hyperion {

struct StreamingCellCollection
{
    using Iterator = typename FlatMap<Vec2i, Handle<StreamingCell>>::Iterator;
    using ConstIterator = typename FlatMap<Vec2i, Handle<StreamingCell>>::ConstIterator;

    FlatMap<Vec2i, Handle<StreamingCell>> cells;
    Vec2u dimensions;

    StreamingCellCollection()
        : dimensions(0, 0)
    {
    }

    StreamingCellCollection(const Vec2u& dimensions)
        : dimensions(dimensions)
    {
    }

    Result AddCell(const Handle<StreamingCell>& cell)
    {
        if (!cell.IsValid())
        {
            return HYP_MAKE_ERROR(Error, "Invalid cell handle");
        }

        const Vec2i coord = cell->GetPatchInfo().coord;

        if (cells.Contains(coord))
        {
            return HYP_MAKE_ERROR(Error, "Cell already exists at coordinate ({}, {})", coord.x, coord.y);
        }

        cells.Insert(coord, cell);

        return {};
    }

    Result RemoveCell(const Handle<StreamingCell>& cell)
    {
        if (!cell.IsValid())
        {
            return HYP_MAKE_ERROR(Error, "Invalid cell handle");
        }

        const Vec2i coord = cell->GetPatchInfo().coord;

        if (!cells.Contains(coord))
        {
            return HYP_MAKE_ERROR(Error, "Cell does not exist at coordinate ({}, {})", coord.x, coord.y);
        }

        cells.Erase(coord);

        return {};
    }

    Handle<StreamingCell> GetCell(const Vec2i& coord) const
    {
        auto it = cells.Find(coord);

        if (it != cells.End())
        {
            return it->second;
        }

        return Handle<StreamingCell>::empty;
    }

    HYP_DEF_STL_BEGIN_END(cells.Begin(), cells.End())
};

} // namespace hyperion

#endif
