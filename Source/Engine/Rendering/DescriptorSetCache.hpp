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
#include <Core/Containers/FlatMap.hpp>

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
    using AllocationsMap = TFlatMap<uint64, Array<DescriptorSetRef, RenderAllocator>, RenderAllocator>;

    AllocationsMap m_allocsByLayout;

    struct AllocatedDescriptorSet
    {
        uint32 frameCounter; // last used frame counter
        DescriptorSetRef descriptorSet;
    };

    Array<AllocatedDescriptorSet, RenderAllocator> m_descriptorSetsInUse;
};

} // namespace Hyperion
