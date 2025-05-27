/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_TERRAIN_WORLD_GRID_PLUGIN_HPP
#define HYPERION_TERRAIN_WORLD_GRID_PLUGIN_HPP

#include <scene/world_grid/WorldGridPlugin.hpp>

#include <core/Handle.hpp>

namespace hyperion {

class Material;
class NoiseCombinator;

HYP_CLASS()

class HYP_API TerrainWorldGridPlugin : public WorldGridPlugin
{
    HYP_OBJECT_BODY(TerrainWorldGridPlugin);

public:
    TerrainWorldGridPlugin();
    virtual ~TerrainWorldGridPlugin() override;

    virtual void Initialize() override;
    virtual void Shutdown() override;
    virtual void Update(GameCounter::TickUnit delta) override;

    virtual UniquePtr<WorldGridPatch> CreatePatch(const WorldGridPatchInfo& patch_info) override;

protected:
    UniquePtr<NoiseCombinator> m_noise_combinator;
    Handle<Material> m_material;
};

} // namespace hyperion

#endif
