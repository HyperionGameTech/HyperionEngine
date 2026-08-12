/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#include <Rendering/Vertex.hpp>

#include <Core/Utilities/EnumFlags.hpp>

#include <Core/Reflection/Enum.hpp>

#ifndef HYP_TOOL
#include <Vertex.generated.inl>
#endif

namespace Hyperion {

String VertexInputLayoutDesc::GetDebugString() const
{
    return EnumToString(EnumFlags<VertexType>(mask));
}

} // namespace Hyperion
