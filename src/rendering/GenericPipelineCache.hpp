/* Copyright (c) 2026 No Tomorrow Games. All rights reserved. */

#pragma once

#include <core/containers/Array.hpp>
#include <core/containers/HashMap.hpp>
#include <core/containers/SparsePagedArray.hpp>

#include <core/threading/Mutex.hpp>

#include <rendering/RenderObject.hpp>
#include <rendering/Shared.hpp>

namespace Hyperion {

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

    ~GenericPipelineCache()
    {
        Clear();
    }

    GenericPipelineCache(const GenericPipelineCache&) = delete;
    GenericPipelineCache& operator=(const GenericPipelineCache&) = delete;

    /*! \brief Gets or creates a pipeline based on the provided shader definition.
     *  \param shaderDefinition The shader definition to use
     *  \return Reference to the pipeline (may not be created yet if async creation is pending)
     */
    PipelineType* GetOrCreate(const ShaderDefinition& shaderDefinition);

    /*! \brief Finds an existing pipeline by shader definition, returns nullptr if not found. */
    PipelineType* Find(const ShaderDefinition& shaderDefinition) const;

    /*! \brief Runs cleanup cycle to remove unused pipelines.
     *  \param maxIter Maximum number of pipelines to check this cycle
     *  \return Number of iterations performed
     */
    int RunCleanupCycle(int maxIter = 10);

    /*! \brief Clears all cached pipelines. */
    void Clear();

protected:
    virtual PipelineRefType MakePipeline(const ShaderDefinition& shaderDefinition) = 0;

private:
    struct CachedPipeline
    {
        PipelineRefType pipeline;
        HashCode key;
    };

    using PipelineMap = HashMap<HashCode, SizeType, NodeAllocator<RenderAllocator>>;
    using PipelineStorage = SparsePagedArray<CachedPipeline, 64, RenderAllocator>;

    PipelineStorage m_pipelines;
    PipelineMap m_keyToIndex;

    IdGenerator m_idGenerator;
    
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
    ComputePipelineRef MakePipeline(const ShaderDefinition& shaderDefinition) override;
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
    RayTracingPipelineRef MakePipeline(const ShaderDefinition& shaderDefinition) override;
};

} // namespace Hyperion
