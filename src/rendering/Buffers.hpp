/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <core/memory/MemoryPool.hpp>
#include <core/memory/Pimpl.hpp>

#include <core/threading/DataRaceDetector.hpp>

#include <core/containers/String.hpp>

#include <core/utilities/Range.hpp>
#include <core/utilities/TypeInfoFwd.hpp>

#include <core/Defines.hpp>

#include <rendering/Shared.hpp>
#include <rendering/RenderObject.hpp>
#include <rendering/RenderGpuBuffer.hpp>
#include <rendering/RenderFrame.hpp>

#include <core/math/Mat4f.hpp>

#include <core/Constants.hpp>
#include <core/Types.hpp>

namespace hyperion {

struct alignas(16) ParticleShaderData
{
    Vec4f position;   //   4 x 4 = 16
    Vec4f velocity;   // + 4 x 4 = 32
    Vec4f color;      // + 4 x 4 = 48
    Vec4f attributes; // + 4 x 4 = 64
};

static_assert(sizeof(ParticleShaderData) == 64);

struct alignas(16) GaussianSplattingInstanceShaderData
{
    Vec4f position; //   4 x 4 = 16
    Vec4f rotation; // + 4 x 4 = 32
    Vec4f scale;    // + 4 x 4 = 48
    Vec4f color;    // + 4 x 4 = 64
};

static_assert(sizeof(GaussianSplattingInstanceShaderData) == 64);

struct GaussianSplattingSceneShaderData
{
    Mat4f modelMatrix;
};

static_assert(sizeof(GaussianSplattingSceneShaderData) == 64);

struct CubemapUniforms
{
    Mat4f projectionMatrices[6];
    Mat4f viewMatrices[6];
};

static_assert(sizeof(CubemapUniforms) % 256 == 0);

struct ImmediateDrawShaderData
{
    Mat4f transform;
    uint32 colorPacked;
    uint32 envProbeType;
    uint32 envProbeIndex;
    uint32 idx; // ~0u == culled
};

static_assert(sizeof(ImmediateDrawShaderData) == 80);

struct SH9Buffer
{
    Vec4f values[16];
};

static_assert(sizeof(SH9Buffer) == 256);

struct SHTile
{
    Vec4f coeffsWeights[9];
};

static_assert(sizeof(SHTile) == 144);

struct VoxelUniforms
{
    Vec4f extent;
    Vec4f aabbMax;
    Vec4f aabbMin;
    Vec4u dimensions; // num mipmaps stored in w component
};

static_assert(sizeof(VoxelUniforms) == 64);

struct BlueNoiseBuffer
{
    Vec4i sobol256spp256d[256 * 256 / 4];
    Vec4i scramblingTile[128 * 128 * 8 / 4];
    Vec4i rankingTile[128 * 128 * 8 / 4];
};

struct RTRadianceUniforms
{
    uint32 numBoundLights;
    uint32 rayOffset; // for lightmapper
    float minRoughness;
    float _pad0;
    Vec2i outputImageResolution;
    float _pad1;
    float _pad2;
    alignas(Vec4f) uint32 lightIndices[16];
};

class StagingBufferPool
{
public:
    HYP_API static StagingBufferPool& GetInstance();

    HYP_API StagingBufferPool();

    HYP_API void Cleanup(uint32 frameIndex);
    HYP_API GpuBufferBase* AcquireStagingBuffer(uint32 frameIndex, uint32 offset, uint32 bufferSize);

private:
    Pimpl<struct StagingBufferPoolImpl> m_impl;
};

class GpuBufferHolderBase
{
protected:
    explicit GpuBufferHolderBase(const TypeInfo* structTypeInfo)
        : m_structTypeInfo(structTypeInfo)
    {
        AssertDebug(m_structTypeInfo != nullptr);
    }

public:
    virtual ~GpuBufferHolderBase();

    HYP_FORCE_INLINE const TypeInfo* GetStructTypeInfo() const
    {
        return m_structTypeInfo;
    }

    virtual SizeType Count() const = 0;

    virtual uint32 NumElementsPerBlock() const = 0;

    HYP_FORCE_INLINE const GpuBufferRef& GetBuffer(uint32 frameIndex) const
    {
        return m_gpuBuffer;
    }

    virtual void MarkDirty(uint32 index) = 0;

    virtual void UpdateBufferSize(uint32 frameIndex) = 0;
    virtual void UpdateBufferData(FrameBase* frame) = 0;

    virtual uint32 AcquireIndex(void** outElementPtr = nullptr) = 0;
    virtual void ReleaseIndex(uint32 index) = 0;

    // Ensures capacity for the given index.
    virtual void EnsureCapacity(uint32 index) = 0;

    void WriteBufferData(uint32 index, const void* ptr, SizeType size)
    {
        AssertDebug(size == TypeInfo_GetSize(*m_structTypeInfo), "Size does not match the expected size! Size = {}, Expected = {}", size, TypeInfo_GetSize(*m_structTypeInfo));

        WriteBufferData_Internal(index, ptr);
    }

    static void WriteBufferData_Static(GpuBufferHolderBase* gpuBufferHolder, uint32 index, void* bufferDataPtr, SizeType bufferSize)
    {
        AssertDebug(gpuBufferHolder != nullptr);
        AssertDebug(bufferSize == TypeInfo_GetSize(*gpuBufferHolder->m_structTypeInfo),
            "Size does not match the expected size! Size = %llu, Expected = %llu",
            bufferSize,
            TypeInfo_GetSize(*gpuBufferHolder->m_structTypeInfo));

        gpuBufferHolder->WriteBufferData_Internal(index, bufferDataPtr);
    }

    virtual void* GetCpuMapping(uint32 index) = 0;

protected:
    void CreateBuffers(GpuBufferType bufferType, SizeType count, SizeType size);
    void CopyToGpuBuffer(
        FrameBase* frame,
        const Array<GpuBufferBase*>& stagingBuffers,
        const Array<uint32>& chunkStarts,
        const Array<uint32>& chunkEnds);

    virtual void WriteBufferData_Internal(uint32 index, const void* ptr) = 0;

    const TypeInfo* m_structTypeInfo;
    GpuBufferRef m_gpuBuffer;
};

// Specialization for Memory pool init info to allocate larger blocks:
template <class StructType>
struct GpuBufferHolderMemoryPoolInitInfo
{
    static constexpr uint32 numBytesPerBlock = MathUtil::Max(MathUtil::NextPowerOf2(sizeof(StructType)), 1u << 14); // minimum 16KB blocks
    static constexpr uint32 numElementsPerBlock = numBytesPerBlock / sizeof(StructType);
    static constexpr uint32 numInitialElements = numElementsPerBlock;
};

template <class StructType>
class GpuBufferHolderMemoryPool final : public MemoryPool<StructType, GpuBufferHolderMemoryPoolInitInfo<StructType>>
{
public:
    using Base = MemoryPool<StructType, GpuBufferHolderMemoryPoolInitInfo<StructType>>;

    GpuBufferHolderMemoryPool(Name poolName, uint32 initialCount = Base::InitInfo::numInitialElements)
        : Base(poolName, initialCount, /* createInitialBlocks */ true, /* blockInitCtx */ nullptr)
    {
        for (Range<uint32>& dirtyRange : m_dirtyRanges)
        {
            dirtyRange = { 0, MathUtil::Max(initialCount, 1) };
        }
    }

    HYP_FORCE_INLINE void MarkDirty(uint32 index)
    {
        for (auto& it : m_dirtyRanges)
        {
            it |= { index, index + 1 };
        }
    }

    void SetElement(uint32 index, const StructType& value)
    {
        Base::SetElement(index, value);

        MarkDirty(index);
    }

    void EnsureGpuBufferCapacity(const GpuBufferRef& buffer, uint32 frameIndex)
    {
        bool wasResized = false;
        HYP_GFX_ASSERT(buffer->EnsureCapacity(Base::NumAllocatedElements() * sizeof(StructType), &wasResized));

        if (wasResized)
        {
            // if resized, we need to copy all data again
            m_dirtyRanges[frameIndex].SetStart(0);
            m_dirtyRanges[frameIndex].SetEnd(Base::NumAllocatedElements());
        }
    }

    void BuildStagingBuffers(uint32 frameIndex, Array<GpuBufferBase*>& outStagingBuffers, Array<uint32>& outChunkStarts, Array<uint32>& outChunkEnds)
    {
        HYP_MT_CHECK_READ(m_dataRaceDetector);

        if (!m_dirtyRanges[frameIndex])
        {
            return;
        }

        const uint32 rangeStart = m_dirtyRanges[frameIndex].GetStart();
        const uint32 rangeEnd = m_dirtyRanges[frameIndex].GetEnd();

        const uint32 numBlocks = Base::m_numBlocks.Get(MemoryOrder::ACQUIRE);

        // Collect all dirty blocks first
        struct DirtyBlockInfo
        {
            uint32 blockIndex;
            uint32 bufferOffset;
            uint32 bufferSize;
            void* dataPtr;
        };

        Array<DirtyBlockInfo> dirtyBlocks;
        dirtyBlocks.Reserve((rangeEnd - rangeStart) / Base::numElementsPerBlock + 1);

        typename LinkedList<typename Base::Block>::Iterator blockIt = Base::m_blocks.Begin();
        typename LinkedList<typename Base::Block>::Iterator endIt = Base::m_blocks.End();

        for (uint32 blockIndex = 0; blockIndex < numBlocks && blockIt != endIt; ++blockIndex, ++blockIt)
        {
            if (blockIndex < rangeStart / Base::numElementsPerBlock)
            {
                continue;
            }

            if (blockIndex * Base::numElementsPerBlock >= rangeEnd)
            {
                break;
            }

            const uint32 offset = blockIndex * Base::numElementsPerBlock;
            const uint32 bufferOffset = offset * uint32(sizeof(StructType));
            const uint32 bufferSize = Base::numElementsPerBlock * uint32(sizeof(StructType));

            dirtyBlocks.PushBack({ blockIndex,
                bufferOffset,
                bufferSize,
                blockIt->buffer.GetPointer() });
        }

        if (dirtyBlocks.Empty())
        {
            m_dirtyRanges[frameIndex].Reset();
            return;
        }

        // Batch blocks into staging buffers, packing non-contiguous ranges to minimize staging buffer count
        // Strategy: Fill staging buffers up to a size limit, allowing gaps for non-contiguous blocks
        static constexpr uint32 maxStagingBufferSize = 1u << 20; // 1MB max per staging buffer
        static constexpr uint32 maxGapSize = 1u << 16;           // 64KB max gap to tolerate (wastes some bandwidth but reduces buffer count)

        struct StagingBatch
        {
            Array<uint32> blockIndices; // indices into dirtyBlocks
            uint32 minOffset = ~0u;     // minimum buffer offset in this batch
            uint32 maxOffset = 0;       // maximum buffer offset (exclusive end) in this batch

            uint32 GetTotalSize() const
            {
                return maxOffset - minOffset;
            }

            uint32 GetDataSize(const Array<DirtyBlockInfo>& dirtyBlocks) const
            {
                uint32 size = 0;
                for (uint32 idx : blockIndices)
                {
                    size += dirtyBlocks[idx].bufferSize;
                }
                return size;
            }
        };

        Array<StagingBatch> batches;
        batches.Reserve(dirtyBlocks.Size()); // worst case: one batch per block

        for (uint32 i = 0; i < dirtyBlocks.Size(); ++i)
        {
            const DirtyBlockInfo& block = dirtyBlocks[i];
            bool addedToBatch = false;

            // Try to add to existing batch
            for (StagingBatch& batch : batches)
            {
                const uint32 newMinOffset = MathUtil::Min(batch.minOffset, block.bufferOffset);
                const uint32 newMaxOffset = MathUtil::Max(batch.maxOffset, block.bufferOffset + block.bufferSize);
                const uint32 newTotalSize = newMaxOffset - newMinOffset;
                const uint32 newDataSize = batch.GetDataSize(dirtyBlocks) + block.bufferSize;
                const uint32 wastedSpace = newTotalSize - newDataSize;

                // Add to this batch if it doesn't exceed size limits and gap isn't too large
                if (newTotalSize <= maxStagingBufferSize && wastedSpace <= maxGapSize)
                {
                    batch.blockIndices.PushBack(i);
                    batch.minOffset = newMinOffset;
                    batch.maxOffset = newMaxOffset;
                    addedToBatch = true;
                    break;
                }
            }

            // Create new batch if couldn't add to existing one
            if (!addedToBatch)
            {
                StagingBatch newBatch;
                newBatch.blockIndices.PushBack(i);
                newBatch.minOffset = block.bufferOffset;
                newBatch.maxOffset = block.bufferOffset + block.bufferSize;
                batches.PushBack(std::move(newBatch));
            }
        }

        // Create staging buffers and copy data for each batch
        for (const StagingBatch& batch : batches)
        {
            const uint32 stagingBufferSize = batch.GetTotalSize();
            const uint32 startOffset = batch.minOffset;

            GpuBufferBase* stagingBuffer = StagingBufferPool::GetInstance().AcquireStagingBuffer(frameIndex, startOffset, stagingBufferSize);
            Assert(stagingBuffer != nullptr && stagingBuffer->IsCreated());

            // Copy each block in the batch to the appropriate offset in the staging buffer
            for (uint32 blockIdx : batch.blockIndices)
            {
                const DirtyBlockInfo& block = dirtyBlocks[blockIdx];
                const uint32 stagingOffset = block.bufferOffset - startOffset;

                stagingBuffer->Copy(stagingOffset, block.bufferSize, block.dataPtr);
            }

            outStagingBuffers.PushBack(stagingBuffer);
            outChunkStarts.PushBack(startOffset);
            outChunkEnds.PushBack(startOffset + stagingBufferSize);
        }

        m_dirtyRanges[frameIndex].Reset();
    }

protected:
    HYP_DECLARE_MT_CHECK(m_dataRaceDetector);

    FixedArray<Range<uint32>, NumFramesInFlight> m_dirtyRanges;
};

template <class StructType, GpuBufferType BufferType>
class GpuBufferHolder final : public GpuBufferHolderBase
{
public:
    explicit GpuBufferHolder(uint32 initialCount = 0)
        : GpuBufferHolderBase(&TypeInfo_ForType<StructType>()),
          m_pool(NAME_FMT("GpuBufferData_{}", TypeInfo_GetName(*m_structTypeInfo)), initialCount)
    {
        GpuBufferHolderBase::CreateBuffers(BufferType, initialCount, sizeof(StructType));
    }

    GpuBufferHolder(const GpuBufferHolder& other) = delete;
    GpuBufferHolder& operator=(const GpuBufferHolder& other) = delete;

    virtual ~GpuBufferHolder() override = default;

    virtual SizeType Count() const override
    {
        return m_pool.NumAllocatedElements();
    }

    virtual uint32 NumElementsPerBlock() const override
    {
        return m_pool.numElementsPerBlock;
    }

    virtual void UpdateBufferSize(uint32 frameIndex) override
    {
        // m_pool.RemoveEmptyBlocks();

        m_pool.EnsureGpuBufferCapacity(m_gpuBuffer, frameIndex);
    }

    virtual void UpdateBufferData(FrameBase* frame) override
    {
        const uint32 frameIndex = frame->GetFrameIndex();

        Array<GpuBufferBase*> stagingBuffers;
        Array<uint32> chunkStarts;
        Array<uint32> chunkEnds;

        m_pool.BuildStagingBuffers(frameIndex, stagingBuffers, chunkStarts, chunkEnds);

        // sanity check, ensure that the chunks are in ascending order
        for (SizeType i = 1; i < chunkStarts.Size(); ++i)
        {
            AssertDebug(chunkStarts[i] >= chunkEnds[i - 1]);
        }

        // sanity check, ensure that the chunks are within bounds of our main gpu buffer
        const SizeType gpuBufferSize = m_gpuBuffer->Size();

        for (SizeType i = 0; i < chunkStarts.Size(); ++i)
        {
            AssertDebug(chunkStarts[i] < gpuBufferSize);
            AssertDebug(chunkEnds[i] <= gpuBufferSize);
        }

        GpuBufferHolderBase::CopyToGpuBuffer(frame, stagingBuffers, chunkStarts, chunkEnds);
    }

    virtual void MarkDirty(uint32 index) override
    {
        m_pool.MarkDirty(index);
    }

    HYP_FORCE_INLINE uint32 AcquireIndex(StructType** outElementPtr)
    {
        return m_pool.AcquireIndex(outElementPtr);
    }

    virtual uint32 AcquireIndex(void** outElementPtr = nullptr) override
    {
        StructType* elementPtr;
        const uint32 index = m_pool.AcquireIndex(&elementPtr);

        if (outElementPtr != nullptr)
        {
            *outElementPtr = elementPtr;
        }

        return index;
    }

    virtual void ReleaseIndex(uint32 batchIndex) override
    {
        return m_pool.ReleaseIndex(batchIndex);
    }

    virtual void EnsureCapacity(uint32 index) override
    {
        m_pool.EnsureCapacity(index);
    }

    HYP_FORCE_INLINE void WriteBufferData(uint32 index, const StructType& value)
    {
        m_pool.SetElement(index, value);
    }

    virtual void* GetCpuMapping(uint32 index) override
    {
        AssertDebug(index < m_pool.NumAllocatedElements(), "Index out of bounds! Index = {}, Size = {}", index, m_pool.NumAllocatedElements());

        return &m_pool.GetElement(index);
    }

private:
    virtual void WriteBufferData_Internal(uint32 index, const void* bufferDataPtr) override
    {
        WriteBufferData(index, *reinterpret_cast<const StructType*>(bufferDataPtr));
    }

    GpuBufferHolderMemoryPool<StructType> m_pool;
};

} // namespace hyperion