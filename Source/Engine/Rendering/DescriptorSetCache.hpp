/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#pragma once

#include <Core/HashCode.hpp>
#include <Core/Types.hpp>

#include <Core/Containers/Array.hpp>
#include <Core/Containers/Map.hpp>

#include <Rendering/RenderTypes.hpp>
#include <Rendering/RenderMemory.hpp>

#include <Rendering/DescriptorSet.hpp>

namespace Hyperion {

class DescriptorSetCache
{
public:
    DescriptorSetCache();
    DescriptorSetCache(const DescriptorSetCache&) = delete;
    DescriptorSetCache& operator=(const DescriptorSetCache&) = delete;
    ~DescriptorSetCache();

    void OnFrameStart();
    void OnFrameEnd();

    DescriptorSet* GetOrCreate(const DescriptorSetLayout& layout);

private:
    struct AllocatedDescriptorSet
    {
        uint32 frameCounter = UINT32_MAX; // last used frame counter
        DescriptorSetRef descriptorSet;
    };
    
    using AllocationsMap = Map<uint64, Array<AllocatedDescriptorSet, RenderAllocator>, RenderAllocator>;

    AllocationsMap m_allocsByLayout;

    Array<AllocatedDescriptorSet, RenderAllocator> m_descriptorSetsInUse;
};

} // namespace Hyperion
