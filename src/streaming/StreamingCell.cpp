/* Copyright (c) 2025 No Tomorrow Games. All rights reserved. */

#include <streaming/StreamingCell.hpp>

#include <scene/world_grid/WorldGrid.hpp>

namespace hyperion {

BoundingBox StreamingCell::GetBoundingBox_Impl() const
{
    const Vec3f min = {
        m_world_grid->GetParams().offset.x + (float(m_cell_info.coord.x) - 0.5f) * (float(m_cell_info.extent.x) - 1.0f) * m_cell_info.scale.x,
        m_world_grid->GetParams().offset.y,
        m_world_grid->GetParams().offset.z + (float(m_cell_info.coord.y) - 0.5f) * (float(m_cell_info.extent.y) - 1.0f) * m_cell_info.scale.z
    };

    const Vec3f max = min + Vec3f(m_cell_info.extent) * m_cell_info.scale;

    return BoundingBox(min, max);
}

} // namespace hyperion
