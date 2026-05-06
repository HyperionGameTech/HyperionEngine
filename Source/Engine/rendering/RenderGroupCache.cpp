/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#include <RenderingPch.hpp>

#include <rendering/RenderGroupCache.hpp>

namespace Hyperion {

RenderGroupCache::RenderGroupCache() = default;

RenderGroupCache::~RenderGroupCache() = default;

RenderableAttributeHandle RenderGroupCache::GetOrCreate(const RenderableAttributeSet& attributes)
{
    AssertOnThread(g_renderThread);

    const HashCode hc = attributes.GetHashCode();

    RenderableAttributeHandle outHandle;

    typename HashMap<HashCode, Array<uint32, RenderAllocator>, RenderAllocator>::Iterator it = m_lookupByHash.End();

    auto CreateIfExists = [&]() -> bool
    {
        it = m_lookupByHash.Find(hc);

        if (it != m_lookupByHash.End())
        {
            for (const uint32 index : it->second)
            {
                if (m_entries[index] == attributes)
                {
                    outHandle = RenderableAttributeHandle::Create(index, attributes.GetMaterialAttributes().bucket);
                    return true;
                }
            }
        }

        return false;
    };

    if (CreateIfExists())
        return outHandle;

    const uint32 index = m_idGenerator.Next() - 1;

    m_entries.Set(index, attributes);

    if (it != m_lookupByHash.End())
    {
        it->second.PushBack(index);
    }
    else
    {
        m_lookupByHash.Set(hc, { index });
    }

    return RenderableAttributeHandle::Create(index, attributes.GetMaterialAttributes().bucket);
}

} // namespace Hyperion
