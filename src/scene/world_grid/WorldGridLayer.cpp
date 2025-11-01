/* Copyright (c) 2025 No Tomorrow Games. All rights reserved. */

#include <scene/world_grid/WorldGridLayer.hpp>
#include <scene/world_grid/WorldGrid.hpp>

#include <streaming/StreamingCell.hpp>

namespace hyperion {

#pragma region WorldGridLayer

Handle<StreamingCell> WorldGridLayer::CreateStreamingCell_Impl(const StreamingCellInfo& cellInfo)
{
    return CreateObject<StreamingCell>(cellInfo);
}

#pragma endregion WorldGridLayer

} // namespace hyperion
