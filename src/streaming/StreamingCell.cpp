/* Copyright (c) 2025 No Tomorrow Games. All rights reserved. */

#include <streaming/StreamingCell.hpp>

#include <scene/world_grid/WorldGrid.hpp>

namespace hyperion {

StreamingCell::StreamingCell(const StreamingCellInfo& cell_info)
    : m_cell_info(cell_info)
{
}

StreamingCell::~StreamingCell()
{
}

} // namespace hyperion
