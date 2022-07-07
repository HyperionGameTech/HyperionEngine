#include "TerrainMeshBuilder.hpp"
#include <Threads.hpp>
#include <math/MathUtil.hpp>

#define MOUNTAIN_SCALE_WIDTH 0.017
#define MOUNTAIN_SCALE_LENGTH 0.017
#define MOUNTAIN_SCALE_HEIGHT 40.0

namespace hyperion::v2 {

TerrainMeshBuilder::TerrainMeshBuilder(const PatchInfo &patch_info)
    : m_patch_info(patch_info)
{
}

void TerrainMeshBuilder::GenerateHeights(Seed seed)
{
    Threads::AssertOnThread(THREAD_TERRAIN);

    auto *simplex       = NoiseFactory::GetInstance()->Capture(NoiseGenerationType::SIMPLEX_NOISE, seed);
    auto *simplex_biome = NoiseFactory::GetInstance()->Capture(NoiseGenerationType::SIMPLEX_NOISE, seed + 1);
    auto *worley        = NoiseFactory::GetInstance()->Capture(NoiseGenerationType::WORLEY_NOISE, seed);
    
    m_heights.resize(m_patch_info.extent.width * m_patch_info.extent.depth);

    for (int z = 0; z < m_patch_info.extent.depth; z++) {
        for (int x = 0; x < m_patch_info.extent.width; x++) {
            const double x_offset = x + (m_patch_info.coord.x * (m_patch_info.extent.width - 1));
            const double z_offset = z + (m_patch_info.coord.y * (m_patch_info.extent.depth - 1));

            const double biome_height = simplex_biome->GetNoise(x_offset * 0.6, z_offset * 0.6);
            const double height = (simplex->GetNoise(x_offset, z_offset)) * 30 - 30;
            const double mountain = ((worley->GetNoise((double)x_offset * MOUNTAIN_SCALE_WIDTH, (double)z_offset * MOUNTAIN_SCALE_LENGTH))) * MOUNTAIN_SCALE_HEIGHT;

            const size_t index = ((x + m_patch_info.extent.width) % m_patch_info.extent.width)
                + ((z + m_patch_info.extent.depth) % m_patch_info.extent.depth) * m_patch_info.extent.width;

            m_heights[index] = MathUtil::Lerp(height, mountain, biome_height);
        }
    }

    NoiseFactory::GetInstance()->Release(simplex);
    NoiseFactory::GetInstance()->Release(simplex_biome);
    NoiseFactory::GetInstance()->Release(worley);
}

std::unique_ptr<Mesh> TerrainMeshBuilder::BuildMesh() const
{
    Threads::AssertOnThread(THREAD_TERRAIN);

    std::vector<Vertex> vertices     = BuildVertices();
    std::vector<Mesh::Index> indices = BuildIndices();

    auto mesh = std::make_unique<Mesh>(
        vertices,
        indices,
        renderer::static_mesh_vertex_attributes | renderer::skeleton_vertex_attributes // for now
    );

    mesh->CalculateNormals();
    mesh->CalculateTangents();

    return mesh;
}

std::vector<Vertex> TerrainMeshBuilder::BuildVertices() const
{
    std::vector<Vertex> vertices;
    vertices.resize(m_patch_info.extent.width * m_patch_info.extent.depth);

    int i = 0;

    for (int z = 0; z < m_patch_info.extent.depth; z++) {
        for (int x = 0; x < m_patch_info.extent.width; x++) {
            Vector3 position(x, m_heights[i], z);
            position *= m_patch_info.scale;

            Vector2 texcoord(
                // static_cast<float>(x % 2),
                // static_cast<float>(z % 2)
               static_cast<float>(x) / static_cast<float>(m_patch_info.extent.width),
               static_cast<float>(z) / static_cast<float>(m_patch_info.extent.depth)
            );

            vertices[i++] = Vertex(position, texcoord);
        }
    }

    return vertices;
}

std::vector<Mesh::Index> TerrainMeshBuilder::BuildIndices() const
{
    std::vector<Mesh::Index> indices;
    indices.resize(6 * (m_patch_info.extent.width - 1) * (m_patch_info.extent.depth - 1));

    int pitch = m_patch_info.extent.width;
    int row = 0;

    int i0 = row;
    int i1 = row + 1;
    int i2 = pitch + i1;
    int i3 = pitch + row;

    int i = 0;

    for (int z = 0; z < m_patch_info.extent.depth - 1; z++) {
        for (int x = 0; x < m_patch_info.extent.width - 1; x++) {
            indices[i++] = i0;
            indices[i++] = i2;
            indices[i++] = i1;
            indices[i++] = i2;
            indices[i++] = i0;
            indices[i++] = i3;

            i0++;
            i1++;
            i2++;
            i3++;
        }

        row += pitch;

        i0 = row;
        i1 = row + 1;
        i2 = pitch + i1;
        i3 = pitch + row;
    }

    return indices;
}

} // namespace hyperion::v2