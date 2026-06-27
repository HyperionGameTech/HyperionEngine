/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <Rendering/RenderTypes.hpp>

#include <Core/Types.hpp>

namespace Hyperion {

class Mesh;
class Material;

class MeshBlasBuilder
{
public:
    static GpuBlasRef Build(Mesh* mesh, Material* material = nullptr);
};

} // namespace Hyperion
