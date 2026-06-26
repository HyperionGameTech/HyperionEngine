/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#pragma once

#include <Core/Reflection/Handle.hpp>

namespace Hyperion {

class Mesh;

class TerrainMeshBuilder
{
public:
    explicit TerrainMeshBuilder(uint32 cellSize);

    TerrainMeshBuilder(const TerrainMeshBuilder& other) = delete;
    TerrainMeshBuilder(TerrainMeshBuilder&& other) noexcept = delete;

    ~TerrainMeshBuilder();

    const Handle<Mesh>& GetMesh();

private:
    uint32 m_cellSize;
    Handle<Mesh> m_mesh;
};

} // namespace Hyperion
