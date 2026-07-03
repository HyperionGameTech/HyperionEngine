/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#include <RenderingPch.hpp>

#include <Rendering/DescriptorSetCache.hpp>
#include <Rendering/DescriptorSet.hpp>
#include <Rendering/RenderInterface.hpp>

#include <Rendering/Util/DeletionQueue.hpp>

#include <Core/Threading/Threads.hpp>

namespace Hyperion {

DescriptorSetCache::DescriptorSetCache()
{
}

DescriptorSetCache::~DescriptorSetCache()
{
    for (auto& it : m_allocsByLayout)
    {
        for (AllocatedDescriptorSet& allocated : it.second)
        {
            EnqueueDeletion(std::move(allocated.descriptorSet));
        }
    }

    for (AllocatedDescriptorSet& allocated : m_descriptorSetsInUse)
    {
        EnqueueDeletion(std::move(allocated.descriptorSet));
    }
}

void DescriptorSetCache::OnFrameStart()
{
    AssertOnThread(g_renderThread);

    const uint32 frameCounter = GetFrameCounter();

    size_t chompIndexStart = SIZE_MAX;

    // recycle descriptor sets no longer in use
    for (auto it = m_descriptorSetsInUse.Begin(); it != m_descriptorSetsInUse.End(); ++it)
    {
        AllocatedDescriptorSet& allocated = *it;
        AssertDebug(frameCounter >= allocated.frameCounter);
        
        if (frameCounter - allocated.frameCounter < NumFramesInFlight)
        {
            break;
        }

        const HashCode layoutHashCode = allocated.descriptorSet->GetLayout().GetHashCode();

        auto mapIt = m_allocsByLayout.Find(layoutHashCode.Value());
        Assert(mapIt != m_allocsByLayout.End());

        auto& recycledElems = mapIt->second;
        // @TODO: Use LowerBound() or change to SortedArray sorted by last used frame (lower first)
        // then we can stop iterating a specific layouts' recycled sets in OnFrameEnd()
        // on the first one that is >= currFrame - NumFramesBeforeDiscard
        recycledElems.PushBack(std::move(allocated));

        chompIndexStart = m_descriptorSetsInUse.IndexOf(it) + 1;
    }

    if (chompIndexStart != SIZE_MAX)
    {
        auto tmp = std::move(m_descriptorSetsInUse);
        m_descriptorSetsInUse.Resize(0);
        m_descriptorSetsInUse.Concat(tmp.ToSpan().Slice(chompIndexStart, tmp.Size() - chompIndexStart));
    }
}

void DescriptorSetCache::OnFrameEnd()
{
    AssertOnThread(g_renderThread);

    // Remove unused allocs after being unused in a while.
    static constexpr uint32 NumFramesBeforeDiscard = 2000;

    const uint32 frameCounter = GetFrameCounter();

    // Note, we don't destroy empty layout slots, as we may need them again for recycling
    // used descriptor sets
    for (auto layoutIt = m_allocsByLayout.Begin(); layoutIt != m_allocsByLayout.End(); ++layoutIt)
    {
        auto& list = layoutIt->second;

        for (auto jt = list.Begin(); jt != list.End();)
        {
            AllocatedDescriptorSet& allocated = *jt;
            AssertDebug(frameCounter >= allocated.frameCounter);
            
            if (frameCounter - allocated.frameCounter >= NumFramesBeforeDiscard)
            {
                // we don't need to enqueue deletion, it isn't used by the gpu.
                // We can just delete it by means of Erase(), as NumFramesBeforeDiscard is AT LEAST NumFramesInFlight
                // And if it isn't... Then that's a problem!
                static_assert(NumFramesBeforeDiscard >= NumFramesInFlight);

                jt = list.Erase(jt);

                continue;
            }

            ++jt;
        }
    }
}

DescriptorSet* DescriptorSetCache::GetOrCreate(const DescriptorSetLayout& layout)
{
    AssertOnThread(g_renderThread);

    const HashCode layoutHashCode = layout.GetHashCode();
    const uint64 layoutHashCodeValue = layoutHashCode.Value();

    auto mapIt = m_allocsByLayout.Find(layoutHashCodeValue);

    if (mapIt == m_allocsByLayout.End())
    {
        mapIt = m_allocsByLayout.Insert(layoutHashCodeValue, {}).first;
    }

    if (mapIt->second.Any())
    {
        auto it = mapIt->second.Begin();

        AllocatedDescriptorSet& allocated = *it;

        AllocatedDescriptorSet& newAllocated = m_descriptorSetsInUse.EmplaceBack(std::move(allocated));
        newAllocated.frameCounter = GetFrameCounter(); // refresh frame counter

        mapIt->second.Erase(it);

        return newAllocated.descriptorSet;
    }

    // need to allocate new descriptor set
    AllocatedDescriptorSet& allocated = m_descriptorSetsInUse.EmplaceBack();
    allocated.frameCounter = GetFrameCounter();
    allocated.descriptorSet = RI.MakeDescriptorSet(layout);

    return allocated.descriptorSet;
}

} // namespace Hyperion
