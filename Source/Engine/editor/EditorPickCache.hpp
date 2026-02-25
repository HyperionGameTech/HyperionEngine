/* Copyright (c) 2016-2026 Andrew J. MacDonald. All rights reserved. */

#pragma once

#include <Core/Types.hpp>
#include <Core/Defines.hpp>

#include <Core/containers/HashMap.hpp>
#include <Core/containers/Array.hpp>

#include <Core/memory/Pimpl.hpp>
#include <Core/memory/pool/Pool.hpp>

#include <Core/threading/Mutex.hpp>

#include <Core/math/Vector3.hpp>

namespace Hyperion {

class Mesh;
class RenderProxyList;

HYP_API extern Pool* g_editorPickCachePool;
using EpcAllocator = AllocatorInstance<Pool, &g_editorPickCachePool>;

struct EditorPickCacheEntry
{
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
    void PutEntry(const Mesh* mesh);
    void RemoveEntry(const Mesh* mesh);
    EditorPickCacheEntry* GetEntry(const Mesh* mesh);
    void Clear();

    void Update(float delta);

private:
    bool EvictEntries(SizeType bytesNeeded);
    bool HasFreeSpace(SizeType bytes);

    Pimpl<struct EditorPickCacheImpl> m_impl;
};

} // namespace Hyperion
