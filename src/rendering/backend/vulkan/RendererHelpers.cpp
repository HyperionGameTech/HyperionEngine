#include "RendererHelpers.hpp"

#include <math/MathUtil.hpp>

namespace hyperion {
namespace renderer {
namespace helpers {

uint32_t MipmapSize(uint32_t src_size, int lod)
{
    return MathUtil::Max(src_size >> lod, 1u);
}

VkIndexType ToVkIndexType(DatumType datum_type)
{
    switch (datum_type) {
    case DatumType::UNSIGNED_BYTE:
        return VK_INDEX_TYPE_UINT8_EXT;
    case DatumType::UNSIGNED_SHORT:
        return VK_INDEX_TYPE_UINT16;
    case DatumType::UNSIGNED_INT:
        return VK_INDEX_TYPE_UINT32;
    default:
        AssertThrowMsg(0, "Unsupported datum type to vulkan index type conversion: %d", int(datum_type));
    }
}


} // namespace renderer
} // namespace helpers
} // namespace hyperion