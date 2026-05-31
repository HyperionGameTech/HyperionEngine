/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <Core/containers/Array.hpp>
#include <Core/containers/FixedArray.hpp>
#include <Core/containers/Map.hpp>

#include <Core/threading/Mutex.hpp>
#include <Core/threading/Task.hpp>

#include <Core/utilities/Span.hpp>

#include <rendering/RenderTypes.hpp>
#include <rendering/Shared.hpp>

#include <Core/Constants.hpp>

namespace Hyperion {

class RenderableAttributeSet;
class CachedPipelinesMap;
class Shader;

class GraphicsPipelineCacheHandle
{
    friend class GraphicsPipelineCache;
    friend class CachedPipelinesMap;

    // indirect pointer to the graphics pipeline. we can check if it is alive by dereferencing this pointer.
    // the indirect pointer is valid as long as this cache handle exists. graphics pipelines are destroyed after a certain number of frames if not used,
    // so you need to call IsAlive() to check if the underlying graphics pipeline is still valid and if not, request a new one.
    GraphicsPipelineRef* m_ptr = nullptr;

    // called only by GraphicsPipelineCache, must be called within mutex lock of the cache as it does not perform any locking
    explicit GraphicsPipelineCacheHandle(GraphicsPipelineRef* graphicsPipelinePtr);
    static void UpdateRefCount(GraphicsPipelineCacheHandle& cacheHandle, int delta, bool lock);

public:
    GraphicsPipelineCacheHandle() = default;

    GraphicsPipelineCacheHandle(const GraphicsPipelineCacheHandle&);
    GraphicsPipelineCacheHandle& operator=(const GraphicsPipelineCacheHandle&);

    GraphicsPipelineCacheHandle(GraphicsPipelineCacheHandle&& other) noexcept
        : m_ptr(other.m_ptr)
    {
        other.m_ptr = nullptr;
    }

    GraphicsPipelineCacheHandle& operator=(GraphicsPipelineCacheHandle&& other) noexcept
    {
        if (m_ptr == other.m_ptr)
        {
            return *this;
        }

        m_ptr = other.m_ptr;
        other.m_ptr = nullptr;

        return *this;
    }

    ~GraphicsPipelineCacheHandle();

    HYP_FORCE_INLINE bool IsAlive() const
    {
        return m_ptr && m_ptr->IsValid();
    }

    HYP_FORCE_INLINE const GraphicsPipelineRef& operator*() const
    {
        AssertDebug(IsAlive());
        return *m_ptr;
    }
};

class GraphicsPipelineCache
{
public:
    friend class GraphicsPipelineCacheHandle;

    GraphicsPipelineCache();

    GraphicsPipelineCache(const GraphicsPipelineCache&) = delete;
    GraphicsPipelineCache& operator=(const GraphicsPipelineCache&) = delete;

    GraphicsPipelineCache(GraphicsPipelineCache&&) = delete;
    GraphicsPipelineCache& operator=(GraphicsPipelineCache&&) = delete;

    ~GraphicsPipelineCache();

    /*! \brief Gets or creates a graphics pipeline based on the provided shader, render target descriptor, and attributes.
     *  Returns a pointer to the reference, do not store a strong reference to it as it will be discarded after a certain number of frames if not used.
     *  Instead, use the returned pointer to access the graphics pipeline. It's guaranteed to be valid for at least 10 frames after this method returns.
     */
    void GetOrCreate(
        RenderableAttributeSet& inOutAttributes,
        const FramebufferDesc& framebufferDesc,
        GraphicsPipelineCacheHandle& outCacheHandle);

    void ExpirePipelinesForShader(const Shader* shader);

    int RunCleanupCycle(int maxIter = 10);

private:
    GraphicsPipelineCacheHandle FindGraphicsPipeline(
        const RenderableAttributeSet& attributes,
        const FramebufferDesc& framebufferDesc);

    CachedPipelinesMap* m_cachedPipelines;
    SharedMutex m_mutex;
};

} // namespace Hyperion
