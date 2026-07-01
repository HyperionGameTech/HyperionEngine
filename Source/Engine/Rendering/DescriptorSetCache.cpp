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
        for (auto& jt : it.second)
        {
            EnqueueDeletion(std::move(jt));
        }
    }

    for (auto& it : m_descriptorSetsInUse)
    {
        EnqueueDeletion(std::move(it.descriptorSet));
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
        if (frameCounter - it->frameCounter < NumFramesInFlight)
        {
            break;
        }

        const HashCode layoutHashCode = it->descriptorSet->GetLayout().GetHashCode();

        auto mapIt = m_allocsByLayout.Find(layoutHashCode.Value());
        Assert(mapIt != m_allocsByLayout.End());

        mapIt->second.PushBack(std::move(it->descriptorSet));

        chompIndexStart = m_descriptorSetsInUse.IndexOf(it) + 1;
    }

    if (chompIndexStart != SIZE_MAX)
    {
        auto tmp = std::move(m_descriptorSetsInUse);
        m_descriptorSetsInUse.Clear();
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
            if (frameCounter - (*jt)->frameCounter >= NumFramesBeforeDiscard)
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

        DescriptorSetRef& ds = *it;

        auto& inUseElem = m_descriptorSetsInUse.EmplaceBack();
        inUseElem.frameCounter = GetFrameCounter();
        inUseElem.descriptorSet = std::move(ds);

        mapIt->second.Erase(it);

        return inUseElem.descriptorSet;
    }

    // need to allocate new descriptor set
    DescriptorSetRef newDescriptorSet = RI.MakeDescriptorSet(layout);

    auto& inUseElem = m_descriptorSetsInUse.EmplaceBack();
    inUseElem.frameCounter = GetFrameCounter();
    inUseElem.descriptorSet = std::move(newDescriptorSet);

    return inUseElem.descriptorSet;
}

} // namespace Hyperion
