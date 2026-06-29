/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#pragma once

#include <Rendering/Shared.hpp>

#include <d3d12.h>
#include <d3dx12.h>
#include <wrl.h>

namespace Hyperion {

using Microsoft::WRL::ComPtr;

/*! \brief Stores the root parameter indices for a descriptor set's views and samplers.
 *  If a descriptor set doesn't have views or samplers, the corresponding index is ~0u. */
struct DescriptorSetRootIndices
{
    static constexpr uint32 MaxDynamicEntries = 8;

    uint32 viewRootIndex = ~0u;     // Root parameter index for CBV/SRV/UAV descriptor table
    uint32 samplerRootIndex = ~0u;  // Root parameter index for sampler descriptor table

    // Dynamic entries: root parameter indices for SetGraphicsRootConstantBufferView / SetGraphicsRootShaderResourceView / SetGraphicsRootUnorderedAccessView
    // Stored in the same order as DescriptorSetLayout::GetDynamicElements() (sorted by binding)
    uint32 dynamicEntryCount = 0;
    uint32 dynamicEntryRootParamIndices[MaxDynamicEntries];
};

} // namespace Hyperion
