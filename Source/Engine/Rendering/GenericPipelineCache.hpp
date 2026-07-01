/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#pragma once

#include <Core/Containers/Array.hpp>
#include <Core/Containers/Map.hpp>
#include <Core/Containers/SparsePagedArray.hpp>

#include <Core/Threading/Mutex.hpp>

#include <Rendering/RenderTypes.hpp>
#include <Rendering/Shared.hpp>

namespace Hyperion {

class Shader;

template <class PipelineType>
class GenericPipelineCache
{
public:
    static constexpr uint32 DefaultDiscardFrames = 100;

    using PipelineRefType = Handle<PipelineType>;

    explicit GenericPipelineCache(uint32 discardFrames = DefaultDiscardFrames)
        : m_discardFrames(discardFrames)
    {
    }

    virtual ~GenericPipelineCache()
    {
        Clear();
    }

    GenericPipelineCache(const GenericPipelineCache&) = delete;
    GenericPipelineCache& operator=(const GenericPipelineCache&) = delete;

    /*! \brief Gets or creates a pipeline based on the provided shader definition.
     *  \param shaderDefinition The shader definition to use
     *  \return Reference to the pipeline (may not be created yet if async creation is pending)
     */
    PipelineType* GetOrCreate(Name shaderName, const ShaderPropertySet& properties);

    /*! \brief Finds an existing pipeline by shader definition, returns nullptr if not found. */
    PipelineType* Find(Name shaderName, const ShaderPropertySet& properties) const;

    void ExpirePipelinesForShader(const Shader* shader);

    /*! \brief Runs cleanup cycle to remove unused pipelines.
     *  \param maxIter Maximum number of pipelines to check this cycle
     *  \return Number of iterations performed
     */
    int RunCleanupCycle(int maxIter = 10);

    /*! \brief Clears all cached pipelines. */
    void Clear();

protected:
    virtual PipelineRefType MakePipeline(Name shaderName, const ShaderPropertySet& properties) = 0;

private:
    struct CachedPipeline
    {
        PipelineRefType pipeline;
        HashCode key;
    };

    using PipelineMap = TMap<HashCode, size_t, RenderAllocator>;
    using PipelineStorage = SparsePagedArray<CachedPipeline, 64, RenderAllocator>;

    PipelineStorage m_pipelines;
    PipelineMap m_keyToIndex;

    AtomicIndexAllocator m_indexAllocator;

    typename PipelineStorage::Iterator m_cleanupIterator;

    mutable SharedMutex m_mutex;
    uint32 m_discardFrames;
};

class ComputePipelineCache final : public GenericPipelineCache<ComputePipeline>
{
public:
    using Base = GenericPipelineCache<ComputePipeline>;

    ComputePipelineCache()
        : Base(100)
    {
    }

protected:
    ComputePipelineRef MakePipeline(Name shaderName, const ShaderPropertySet& properties) override;
};

class RayTracingPipelineCache final : public GenericPipelineCache<RayTracingPipeline>
{
public:
    using Base = GenericPipelineCache<RayTracingPipeline>;

    RayTracingPipelineCache()
        : Base(100)
    {
    }

protected:
    RayTracingPipelineRef MakePipeline(Name shaderName, const ShaderPropertySet& properties) override;
};

} // namespace Hyperion
