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

/// Replaces alpha in every mip past 0 with the fraction of mip 0 texels at or above \p alphaCutoff that it covers.
/// Cutout shaders dither minified samples on that fraction instead of testing it against the cutoff.
void BuildAlphaCoverageMips(const TextureDesc& desc, ByteBuffer& imageData, float alphaCutoff);

} // namespace TextureUtils

} // namespace Hyperion
