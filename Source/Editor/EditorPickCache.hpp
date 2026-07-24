/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <Core/Types.hpp>
#include <Core/Defines.hpp>

#include <Core/Containers/Map.hpp>
#include <Core/Containers/Array.hpp>

#include <Core/Memory/Pimpl.hpp>
#include <Core/Memory/Pool/Pool.hpp>

#include <Core/Threading/Mutex.hpp>

#include <Core/Math/Vector3.hpp>

#include <Core/Reflection/Handle.hpp>

namespace Hyperion {

class Mesh;
class RenderProxyList;

EDITOR_API extern Pool* g_editorPickCachePool;
using EpcAllocator = AllocatorInstance<Pool, &g_editorPickCachePool>;

struct EditorPickCacheEntry
{
    WeakHandle<Mesh> mesh;
    int residency = 0;
    uint32 frameVisible = 0;
    Array<Vec3f, EpcAllocator> positions;
    Array<uint32, EpcAllocator> indices;
};

class EditorPickCache
{
public:
    EditorPickCache();
    EditorPickCache(const EditorPickCache&) = delete;
    EditorPickCache& operator=(const EditorPickCache&) = delete;
    EditorPickCache(EditorPickCache&&) noexcept = delete;
    EditorPickCache& operator=(EditorPickCache&&) noexcept = delete;
    ~EditorPickCache();

    RenderProxyList& GetRenderProxyList() const;

    bool HasEntry(const Mesh* mesh) const;
    void PutEntry(const Mesh* mesh, bool invalidate = false);
    void RemoveEntry(const Mesh* mesh);
    EditorPickCacheEntry* GetEntry(const Mesh* mesh);
    void Clear();

    void Update(float delta);

private:
    bool EvictEntries(size_t bytesNeeded);
    bool HasFreeSpace(size_t bytes);
    void RemoveEntryAtIndex(uint32 index);

    Pimpl<struct EditorPickCacheImpl> m_impl;
};

} // namespace Hyperion
