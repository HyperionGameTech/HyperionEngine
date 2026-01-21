/* Copyright (c) 2024-2025 No Tomorrow Games. All rights reserved. */

#pragma once

#include <core/HashCode.hpp>

#include <core/config/Config.hpp>

#include <core/utilities/EnumFlags.hpp>

#include <core/math/BoundingBox.hpp>

#include <scene/Entity.hpp>

#include <rendering/RenderCommand.hpp>

namespace Hyperion {

class RenderProxyEnvGrid;

HYP_CLASS()
class HYP_API EnvGrid : public Entity
{
    HYP_OBJECT_BODY(EnvGrid);

public:
    EnvGrid();
    
    EnvGrid(const EnvGrid& other) = delete;
    EnvGrid& operator=(const EnvGrid& other) = delete;

    ~EnvGrid() override;

    virtual void UpdateRenderProxy(RenderProxyEnvGrid* proxy) = 0;
};

} // namespace Hyperion
