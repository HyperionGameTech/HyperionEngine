/* Copyright (c) 2026 No Tomorrow Games. All rights reserved. */

#pragma once

#include <core/HashCode.hpp>
#include <core/Types.hpp>

#include <core/containers/Array.hpp>
#include <core/containers/HashMap.hpp>
#include <core/containers/FlatMap.hpp>

#include <rendering/RenderObject.hpp>
#include <rendering/RenderMemory.hpp>

#include <rendering/DescriptorSet.hpp>

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
    using AllocationsMap = HashMap<HashCode, Array<DescriptorSetRef, RenderAllocator>, NodeAllocator<RenderAllocator>>;

    AllocationsMap m_allocsByLayout;
    
    struct AllocatedDescriptorSet
    {
        uint32 frameCounter; // last used frame counter
        DescriptorSetRef descriptorSet;
    };

    Array<AllocatedDescriptorSet, RenderAllocator> m_descriptorSetsInUse;
};

} // namespace Hyperion
