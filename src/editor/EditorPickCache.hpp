/* Copyright (c) 2025 No Tomorrow Games. All rights reserved. */

#pragma once

#include <core/Types.hpp>
#include <core/Defines.hpp>

#include <core/containers/HashMap.hpp>
#include <core/containers/Array.hpp>

#include <core/memory/Pimpl.hpp>
#include <core/memory/pool/Pool.hpp>

#include <core/threading/Mutex.hpp>

#include <core/math/Vector3.hpp>

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
    static constexpr SizeType MaxMemoryUsageBytes = (32 * 1024 * 1024); // 32 MiB

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

    Pimpl<struct EditorPickCacheImpl> m_impl;
};

} // namespace Hyperion
