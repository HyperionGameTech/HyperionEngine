/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <Scene/world_grid/WorldGridLayer.hpp>

#include <Core/reflection/Handle.hpp>

namespace Hyperion {

class MaterialInstance;
class Scene;

HYP_CLASS()
class ENGINE_API TerrainWorldGridLayer : public WorldGridLayer
{
    HYP_OBJECT_BODY(TerrainWorldGridLayer);

public:
    TerrainWorldGridLayer();
    virtual ~TerrainWorldGridLayer() override;

    HYP_METHOD()
    HYP_FORCE_INLINE const Handle<Scene>& GetScene() const
    {
        return m_scene;
    }

protected:
    HYP_METHOD()
    virtual void Init() override;

    HYP_METHOD()
    virtual void OnAdded_Impl(WorldGrid* worldGrid) override;

    HYP_METHOD()
    virtual void OnRemoved_Impl(WorldGrid* worldGrid) override;

    HYP_METHOD()
    virtual Handle<StreamingCell> CreateStreamingCell_Impl(const StreamingCellInfo& cellInfo) override;

    Handle<Scene> m_scene;
    Handle<MaterialInstance> m_material;
};

} // namespace Hyperion
