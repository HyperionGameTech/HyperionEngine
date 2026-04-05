/* Copyright (c) 2016-2026 Andrew J. MacDonald. All rights reserved. */

#pragma once

#include <Core/HashCode.hpp>
#include <Core/Types.hpp>

#include <Core/containers/Array.hpp>
#include <Core/containers/HashMap.hpp>
#include <Core/containers/FlatMap.hpp>

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
    using AllocationsMap = HashMap<HashCode, Array<DescriptorSetRef, RenderAllocator>, RenderAllocator>;

    AllocationsMap m_allocsByLayout;
    
    struct AllocatedDescriptorSet
    {
        uint32 frameCounter; // last used frame counter
        DescriptorSetRef descriptorSet;
    };

    Array<AllocatedDescriptorSet, RenderAllocator> m_descriptorSetsInUse;
};

} // namespace Hyperion
