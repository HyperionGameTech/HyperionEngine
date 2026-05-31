/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <Core/HashCode.hpp>

#include <Core/Config/Config.hpp>

#include <Core/Utilities/EnumFlags.hpp>

#include <Core/Math/BoundingBox.hpp>

#include <Scene/Entity.hpp>

#include <Rendering/RenderCommand.hpp>

namespace Hyperion {

class RenderProxyEnvGrid;

HYP_CLASS()
class ENGINE_API EnvGrid : public Entity
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
