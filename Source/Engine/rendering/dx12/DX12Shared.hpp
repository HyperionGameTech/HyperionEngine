/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#pragma once

#include <d3d12.h>
#include <d3dx12.h>
#include <wrl.h>

namespace Hyperion {

using Microsoft::WRL::ComPtr;

/*! \brief Stores the root parameter indices for a descriptor set's views and samplers.
 *  If a descriptor set doesn't have views or samplers, the corresponding index is ~0u. */
struct DescriptorSetRootIndices
{
    uint32 viewRootIndex = ~0u;     // Root parameter index for CBV/SRV/UAV descriptor table
    uint32 samplerRootIndex = ~0u;  // Root parameter index for sampler descriptor table
};

} // namespace Hyperion
