/* Copyright (c) 2026 No Tomorrow Games. All rights reserved. */

#include <RenderingPch.hpp>

#include <rendering/DescriptorSetCache.hpp>
#include <rendering/DescriptorSet.hpp>
#include <rendering/RenderBackend.hpp>
#include <rendering/RenderInterface.hpp>

#include <rendering/util/SafeDeleter.hpp>

#include <core/threading/Threads.hpp>

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
            SafeDelete(std::move(jt));
        }
    }

    for (auto& it : m_descriptorSetsInUse)
    {
        SafeDelete(std::move(it.descriptorSet));
    }
}

void DescriptorSetCache::OnFrameStart()
{
    AssertOnThread(g_renderThread);

    const uint32 frameCounter = RenderApi::GetFrameCounter();

    SizeType chompIndexStart = SizeType(-1);

    // recycle descriptor sets no longer in use
    for (auto it = m_descriptorSetsInUse.Begin(); it != m_descriptorSetsInUse.End(); ++it)
    {
        if (frameCounter - it->frameCounter < NumFramesInFlight)
        {
            break;
        }

        const HashCode layoutHashCode = it->descriptorSet->GetLayout().GetHashCode();

        auto mapIt = m_allocsByLayout.Find(layoutHashCode);
        AssertDebug(mapIt != m_allocsByLayout.End());

        mapIt->second.PushBack(std::move(it->descriptorSet));

        chompIndexStart = m_descriptorSetsInUse.IndexOf(it) + 1;
    }

    if (chompIndexStart != SizeType(-1))
    {
        auto tmp = std::move(m_descriptorSetsInUse);
        m_descriptorSetsInUse.Clear();
        m_descriptorSetsInUse.Concat(tmp.ToSpan().Slice(chompIndexStart, tmp.Size() - chompIndexStart));
    }
}

void DescriptorSetCache::OnFrameEnd()
{
    AssertOnThread(g_renderThread);

    // TODO: Remove unused allocs after a certain no. of frames!
}

DescriptorSet* DescriptorSetCache::GetOrCreate(const DescriptorSetLayout& layout)
{
    AssertOnThread(g_renderThread);

    const HashCode layoutHashCode = layout.GetHashCode();

    auto mapIt = m_allocsByLayout.Find(layoutHashCode);

    if (mapIt == m_allocsByLayout.End())
    {
        mapIt = m_allocsByLayout.Insert(layoutHashCode, {}).first;
    }

    if (mapIt->second.Any())
    {
        for (auto it = mapIt->second.Begin(); it != mapIt->second.End();)
        {
            DescriptorSetRef& ds = *it;

            /*if (*ds->GetLayout().GetDeclaration() == *layout.GetDeclaration())
            {*/
                auto& inUseElem = m_descriptorSetsInUse.EmplaceBack();
                inUseElem.frameCounter = RenderApi::GetFrameCounter();
                inUseElem.descriptorSet = std::move(ds);

                mapIt->second.Erase(it);

                return inUseElem.descriptorSet;
            //}

            ++it;
        }
    }

    // need to allocate new descriptor set
    DescriptorSetRef newDescriptorSet = g_renderBackend->MakeDescriptorSet(layout);
    //RendererResult createResult = newDescriptorSet->Create();
    //Assert(!createResult.HasError(), "Failed to create new descriptor set! Error: {}", createResult.GetError().GetMessage());
    
    auto& inUseElem = m_descriptorSetsInUse.EmplaceBack();
    inUseElem.frameCounter = RenderApi::GetFrameCounter();
    inUseElem.descriptorSet = std::move(newDescriptorSet);

    return inUseElem.descriptorSet;
}

} // namespace Hyperion
