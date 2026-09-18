/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#pragma once

#include <Core/Containers/Array.hpp>

#include <Core/Utilities/Result.hpp>

#include <Rendering/Mesh.hpp>

namespace Hyperion {

struct GeneratedMeshLod
{
    MeshLodDesc desc;
    Array<float> vertexData;
    Array<uint32> indices;
};

struct MeshLodGenerationResult
{
    ///Generated LODs, starting at LOD 1.
    Array<GeneratedMeshLod> lods;

    uint64 sourceDataHash = 0;
};

class ENGINE_API MeshLodGenerator final
{
public:
    static bool IsSupported();

    static bool CanGenerate(const Mesh* mesh);

    static TResult<MeshLodGenerationResult> Generate(const Mesh* mesh, const MeshLodGenerationSettings& settings);
    static Result Apply(Mesh* mesh, const MeshLodGenerationResult& result);
};

} // namespace Hyperion
