/* Copyright (c) 2025 No Tomorrow Games. All rights reserved. */

#include <streaming/StreamingCell.hpp>

#include <scene/world_grid/WorldGrid.hpp>

namespace hyperion {

StreamingCell::StreamingCell(const StreamingCellInfo& cellInfo)
    : m_cellInfo(cellInfo)
{
}

StreamingCell::~StreamingCell()
{
}

} // namespace hyperion
