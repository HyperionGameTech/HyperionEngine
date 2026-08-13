/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <Scene/WorldGrid/WorldGridLayer.hpp>

#include <Core/Reflection/Handle.hpp>

namespace Hyperion {

class Material;
class Mesh;
class Scene;

HYP_CLASS()
class ENGINE_API TerrainWorldGridLayer : public WorldGridLayer
{
    HYP_OBJECT_BODY(TerrainWorldGridLayer);

public:
    TerrainWorldGridLayer();
    
    explicit TerrainWorldGridLayer(Name name, const WorldGridLayerInfo& layerInfo = {});

    virtual ~TerrainWorldGridLayer() override;

    HYP_METHOD()
    HYP_FORCE_INLINE const Handle<Scene>& GetScene() const
    {
        return m_scene;
    }

protected:
    virtual void OnAdded(WorldGrid* worldGrid) override;
    virtual void OnRemoved(WorldGrid* worldGrid) override;

    virtual Handle<StreamingCell> CreateStreamingCell(const StreamingCellInfo& cellInfo) override;

    Handle<Scene> m_scene;
    Handle<Mesh> m_mesh;
    Handle<Material> m_material;
};

} // namespace Hyperion
