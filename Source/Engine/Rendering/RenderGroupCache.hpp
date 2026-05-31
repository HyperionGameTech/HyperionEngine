/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <Core/Containers/Map.hpp>
#include <Core/Containers/Array.hpp>

#include <Core/Utilities/IdGenerator.hpp>

#include <Rendering/RenderableAttributes.hpp>

namespace Hyperion {

/*! Caches RenderableAttributeSet structs using small 32-bit handle for faster copies and less memory usage.
 *   Only usable from render thread or renderer worker threads -- and GetOrCreate() is Render thread only, Get() is usable from
 *   either Render thread or renderer worker threads. Not for use from Sim or other threads. */
class RenderGroupCache
{
public:
    RenderGroupCache();

    RenderGroupCache(const RenderGroupCache&) = delete;
    RenderGroupCache& operator=(const RenderGroupCache&) = delete;

    ~RenderGroupCache();

    RenderableAttributeHandle GetOrCreate(const RenderableAttributeSet& attributes);

    HYP_FORCE_INLINE const RenderableAttributeSet& Get(RenderableAttributeHandle handle) const
    {
        AssertOnThread(g_renderThread | ThreadCategory::THREAD_CATEGORY_TASK);

        AssertDebug(handle.IsValid());
        AssertDebug(m_entries.HasIndex(handle.GetIndex()));

        return m_entries.Get(handle.GetIndex());
    }

private:
    TMap<HashCode, Array<uint32, RenderAllocator>, RenderAllocator> m_lookupByHash;
    SparsePagedArray<RenderableAttributeSet, 1024, RenderAllocator> m_entries;
    IdGenerator m_idGenerator;
};

} // namespace Hyperion
