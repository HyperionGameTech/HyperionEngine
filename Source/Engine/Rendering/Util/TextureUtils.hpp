/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#pragma once

#include <Core/Memory/ByteBuffer.hpp>

#include <Rendering/Shared.hpp>

namespace Hyperion {

// format helpers live with TextureDesc in Shared.hpp
namespace TextureUtils {

/// Rescales alpha in each generated mip so the share of texels passing \p alphaCutoff matches mip 0
void PreserveAlphaCoverage(const TextureDesc& desc, ByteBuffer& imageData, float alphaCutoff);

} // namespace TextureUtils

} // namespace Hyperion
