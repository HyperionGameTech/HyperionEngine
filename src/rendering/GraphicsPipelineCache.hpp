/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <core/containers/Array.hpp>
#include <core/containers/FixedArray.hpp>
#include <core/containers/HashMap.hpp>

#include <core/threading/Mutex.hpp>
#include <core/threading/Task.hpp>

#include <core/utilities/Span.hpp>

#include <rendering/RenderObject.hpp>
#include <rendering/Shared.hpp>

#include <core/Constants.hpp>

namespace hyperion {

class RenderableAttributeSet;
class CachedPipelinesMap;
struct DescriptorTableDeclaration;

class GraphicsPipelineCacheHandle
{
    friend class GraphicsPipelineCache;
    friend class CachedPipelinesMap;

    // indirect pointer to the graphics pipeline. we can check if it is alive by dereferencing this pointer.
    // the indirect pointer is valid as long as this cache handle exists. graphics pipelines are destroyed after a certain number of frames if not used,
    // so you need to call IsAlive() to check if the underlying graphics pipeline is still valid and if not, request a new one.
    GraphicsPipelineRef* m_pRef = nullptr;

    // called only by GraphicsPipelineCache, must be called within mutex lock of the cache as it does not perform any locking
    explicit GraphicsPipelineCacheHandle(GraphicsPipelineRef* pRef);
    static void UpdateRefCount(GraphicsPipelineCacheHandle& cacheHandle, int delta, bool lock);
    
public:
    GraphicsPipelineCacheHandle() = default;

    GraphicsPipelineCacheHandle(const GraphicsPipelineCacheHandle&);
    GraphicsPipelineCacheHandle& operator=(const GraphicsPipelineCacheHandle&);

    GraphicsPipelineCacheHandle(GraphicsPipelineCacheHandle&& other) noexcept
        : m_pRef(other.m_pRef)
    {
        other.m_pRef = nullptr;
    }

    GraphicsPipelineCacheHandle& operator=(GraphicsPipelineCacheHandle&& other) noexcept
    {
        if (m_pRef == other.m_pRef)
        {
            return *this;
        }

        m_pRef = other.m_pRef;
        other.m_pRef = nullptr;

        return *this;
    }

    ~GraphicsPipelineCacheHandle();

    HYP_FORCE_INLINE bool IsAlive() const
    {
        return m_pRef && m_pRef->IsValid();
    }

    HYP_FORCE_INLINE const GraphicsPipelineRef& operator*() const
    {
        AssertDebug(IsAlive());
        return *m_pRef;
    }
};

class GraphicsPipelineCache
{
public:
    friend struct GraphicsPipelineCacheHandle;

    GraphicsPipelineCache();
    GraphicsPipelineCache(const GraphicsPipelineCache&) = delete;
    GraphicsPipelineCache& operator=(const GraphicsPipelineCache&) = delete;
    GraphicsPipelineCache(GraphicsPipelineCache&&) = delete;
    GraphicsPipelineCache& operator=(GraphicsPipelineCache&&) = delete;
    ~GraphicsPipelineCache();

    /*! \brief Gets or creates a graphics pipeline based on the provided shader, descriptor table, framebuffers, and attributes.
     *  Returns a pointer to the reference, do not store a strong reference to it as it will be discarded after a certain number of frames if not used.
     *  Instead, use the returned pointer to access the graphics pipeline. It's guaranteed to be valid for at least 10 frames after this method returns.
     */
    GraphicsPipelineCacheHandle GetOrCreate(
        const ShaderRef& shader,
        const DescriptorTableRef& descriptorTable,
        Span<const FramebufferRef> framebuffers,
        const RenderableAttributeSet& attributes);

    int RunCleanupCycle(int maxIter = 10);

private:
    GraphicsPipelineCacheHandle FindGraphicsPipeline(
        const ShaderRef& shader,
        const DescriptorTableDeclaration& descriptorTableDecl,
        Span<const FramebufferRef> framebuffers,
        const RenderableAttributeSet& attributes);

    CachedPipelinesMap* m_cachedPipelines;
    Mutex m_mutex;
};

} // namespace hyperion
