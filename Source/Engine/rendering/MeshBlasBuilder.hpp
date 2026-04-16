/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <rendering/RenderObject.hpp>

#include <Core/Types.hpp>

namespace Hyperion {

class Mesh;
class MaterialInstance;

class HYP_API MeshBlasBuilder
{
public:
    static GpuBlasRef Build(Mesh* mesh, MaterialInstance* material = nullptr);
};

} // namespace Hyperion
