/* Copyright (c) 2016-2026 Andrew J. MacDonald. All rights reserved. */

#pragma once

#include <Core/HashCode.hpp>

#include <Core/config/Config.hpp>

#include <Core/utilities/EnumFlags.hpp>

#include <Core/math/BoundingBox.hpp>

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
