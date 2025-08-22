/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <core/memory/MemoryPool.hpp>

#include <core/threading/DataRaceDetector.hpp>

#include <core/containers/String.hpp>

#include <core/utilities/Range.hpp>

#include <core/Defines.hpp>

#include <rendering/Shared.hpp>
#include <rendering/RenderObject.hpp>
#include <rendering/RenderGpuBuffer.hpp>

#include <core/math/Matrix4.hpp>

#include <core/Constants.hpp>
#include <core/Types.hpp>

namespace hyperion {

static constexpr SizeType maxProbesInShGridBuffer = g_maxBoundAmbientProbes;

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
    Matrix4 modelMatrix;
};

static_assert(sizeof(GaussianSplattingSceneShaderData) == 64);

struct CubemapUniforms
{
    Matrix4 projectionMatrices[6];
    Matrix4 viewMatrices[6];
};

static_assert(sizeof(CubemapUniforms) % 256 == 0);

struct ImmediateDrawShaderData
{
    Matrix4 transform;
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

struct PendingGpuBufferUpdate
{
    uint32 offset = 0;
    uint32 count = 0;
    GpuBufferBase* stagingBuffer = nullptr;
};

extern HYP_API uint32 RenderApi_GetFrameCounter();

class GpuBufferHolderBase
{
protected:
    template <class T>
    GpuBufferHolderBase(TypeWrapper<T>)
        : m_structTypeId(TypeId::ForType<T>()),
          m_structSize(sizeof(T))
    {
    }

public:
    virtual ~GpuBufferHolderBase();

    HYP_FORCE_INLINE TypeId GetStructTypeId() const
    {
        return m_structTypeId;
    }

    HYP_FORCE_INLINE SizeType GetStructSize() const
    {
        return m_structSize;
    }

    HYP_FORCE_INLINE SizeType GetGpuBufferOffset(uint32 elementIndex) const
    {
        return m_structSize * elementIndex;
    }

    virtual SizeType Count() const = 0;

    virtual uint32 NumElementsPerBlock() const = 0;

    HYP_FORCE_INLINE const GpuBufferRef& GetBuffer(uint32 frameIndex) const
    {
        return m_gpuBuffer;
    }

    HYP_FORCE_INLINE void MarkDirty(uint32 index)
    {
        m_dirtyRange |= { index, index + 1 };
    }

    virtual void UpdateBufferData(FrameBase* frame) = 0;

    virtual uint32 AcquireIndex(void** outElementPtr = nullptr) = 0;
    virtual void ReleaseIndex(uint32 index) = 0;

    // Ensures capacity for the given index.
    virtual void EnsureCapacity(uint32 index) = 0;

    void WriteBufferData(uint32 index, const void* ptr, SizeType size)
    {
        AssertDebug(size == m_structSize, "Size does not match the expected size! Size = {}, Expected = {}", size, m_structSize);

        WriteBufferData_Internal(index, ptr);
    }

    static void WriteBufferData_Static(GpuBufferHolderBase* gpuBufferHolder, uint32 index, void* bufferDataPtr, SizeType bufferSize)
    {
        AssertDebug(gpuBufferHolder != nullptr);
        AssertDebug(bufferSize == gpuBufferHolder->m_structSize,
            "Size does not match the expected size! Size = %llu, Expected = %llu",
            bufferSize,
            gpuBufferHolder->m_structSize);

        gpuBufferHolder->WriteBufferData_Internal(index, bufferDataPtr);
    }

    virtual void* GetCpuMapping(uint32 index) = 0;

protected:
    void CreateBuffers(GpuBufferType type, SizeType count, SizeType size);
    void ApplyPendingUpdates(FrameBase* frame);

    GpuBufferRef CreateStagingBuffer(uint32 size);

    virtual void WriteBufferData_Internal(uint32 index, const void* ptr) = 0;

    TypeId m_structTypeId;
    SizeType m_structSize;

    GpuBufferRef m_gpuBuffer;
    Range<uint32> m_dirtyRange;

    Array<PendingGpuBufferUpdate> m_pendingUpdates;
    
    struct CachedStagingBuffer
    {
        uint32 offset = 0; // offset in the gpu buffer
        uint32 count = 0; // number of elements in the gpu buffer
        uint32 lastFrame = uint32(-1); // frame index this was last used
        GpuBufferRef stagingBuffer;
    };

    Array<CachedStagingBuffer> m_cachedStagingBuffers;
};

template <class StructType>
class GpuBufferHolderMemoryPool final : public MemoryPool<StructType>
{
public:
    using Base = MemoryPool<StructType>;

    GpuBufferHolderMemoryPool(Name poolName, uint32 initialCount = Base::InitInfo::numInitialElements)
        : Base(poolName, initialCount, /* createInitialBlocks */ true, /* blockInitCtx */ nullptr)
    {
    }

    void WriteToStagingBuffer(GpuBufferBase* buffer, uint32 rangeStart, uint32 rangeEnd)
    {
        HYP_MT_CHECK_READ(m_dataRaceDetector);

        HYP_GFX_ASSERT(buffer != nullptr);
        HYP_GFX_ASSERT(rangeStart <= rangeEnd);

        if (rangeStart == rangeEnd)
        {
            return;
        }

        uint32 blockIndex = 0;

        typename LinkedList<typename Base::Block>::Iterator beginIt = Base::m_blocks.Begin();
        typename LinkedList<typename Base::Block>::Iterator endIt = Base::m_blocks.End();

        for (uint32 blockIndex = 0; blockIndex < Base::m_numBlocks.Get(MemoryOrder::ACQUIRE) && beginIt != endIt; ++blockIndex, ++beginIt)
        {
            if (blockIndex < rangeStart / Base::numElementsPerBlock)
            {
                continue;
            }

            if (blockIndex * Base::numElementsPerBlock >= rangeEnd)
            {
                break;
            }

            const SizeType bufferSize = buffer->Size();

            const uint32 index = blockIndex * Base::numElementsPerBlock;

            const SizeType count = MathUtil::Min(Base::numElementsPerBlock, rangeEnd - rangeStart);

            // sanity checks
            AssertDebug(count <= int64(bufferSize / sizeof(StructType)),
                "Buffer does not have enough space for the current number of elements! Buffer size = {}, Required size = {}",
                bufferSize,
                count * sizeof(StructType));

            buffer->Copy(
                0,
                count * sizeof(StructType),
                reinterpret_cast<StructType*>(beginIt->buffer.GetPointer()));
        }
    }

private:
protected:
    HYP_DECLARE_MT_CHECK(m_dataRaceDetector);
};

template <class StructType, GpuBufferType BufferType>
class GpuBufferHolder final : public GpuBufferHolderBase
{
public:
    static constexpr uint32 s_minUpdateBufferSize = 65535;
    static constexpr uint32 s_updateBufferSize = MathUtil::NextMultiple(s_minUpdateBufferSize, sizeof(StructType));

    explicit GpuBufferHolder(uint32 initialCount = 0)
        : GpuBufferHolderBase(TypeWrapper<StructType> {}),
          m_pool(CreateNameFromDynamicString(ANSIString("GfxBuffers_") + TypeNameWithoutNamespace<StructType>().Data()), initialCount)
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

    virtual void UpdateBufferData(FrameBase* frame) override
    {
        Threads::AssertOnThread(g_renderThread);
        HYP_MT_CHECK_READ(m_dataRaceDetector);

        // Create staging buffers

        if (!m_dirtyRange)
        {
            return;
        }

        const uint32 rangeStart = m_dirtyRange.GetStart();
        const uint32 rangeEnd = m_dirtyRange.GetEnd();

        auto getNextPendingUpdate = [this](uint32 offset, uint32 count) -> PendingGpuBufferUpdate*
        {
            PendingGpuBufferUpdate& newUpdate = m_pendingUpdates.EmplaceBack();
            newUpdate.offset = offset; // dst offset in the gpu buffer
            newUpdate.count = count;
            newUpdate.stagingBuffer = GetCachedStagingBuffer(offset, count);

            return &newUpdate;
        };

        for (uint32 i = rangeStart; i < rangeEnd;)
        {
            const uint32 startOffset = i - (i % (s_updateBufferSize / sizeof(StructType)));
            const uint32 endOffset = MathUtil::Min(startOffset + (s_updateBufferSize / sizeof(StructType)), rangeEnd);

            // make a new update
            PendingGpuBufferUpdate* update = getNextPendingUpdate(startOffset * sizeof(StructType), s_updateBufferSize);
            HYP_GFX_ASSERT(update != nullptr);

            m_pool.WriteToStagingBuffer(
                update->stagingBuffer,
                startOffset,
                endOffset);

            i += (endOffset - startOffset);
        }

        ApplyPendingUpdates(frame);

        m_dirtyRange.Reset();
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
        
        m_dirtyRange |= { index, index + 1 };
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

    GpuBufferBase* GetCachedStagingBuffer(uint32 offset, uint32 count)
    {
        GpuBufferRef* cachedStagingBuffer = nullptr;

        for (CachedStagingBuffer& it : m_cachedStagingBuffers)
        {
            if (it.offset == offset && it.count == count)
            {
                it.lastFrame = RenderApi_GetFrameCounter();

                HYP_GFX_ASSERT(it.stagingBuffer != nullptr, "Staging buffer should not be null!");

                return it.stagingBuffer;
            }
        }

        // none found with same offset and count, look for one with 0 count (meaning it was reset last frame and we can re-use it)
        for (CachedStagingBuffer& it : m_cachedStagingBuffers)
        {
            if (it.count == 0)
            {
                it.offset = offset;
                it.count = count;
                it.lastFrame = RenderApi_GetFrameCounter();

                HYP_GFX_ASSERT(it.stagingBuffer != nullptr, "Staging buffer should not be null!");

                return it.stagingBuffer;
            }
        }

        // finally, create a new one if none found
        CachedStagingBuffer& newCachedStagingBuffer = m_cachedStagingBuffers.EmplaceBack();
        newCachedStagingBuffer.offset = offset;
        newCachedStagingBuffer.count = count;
        newCachedStagingBuffer.lastFrame = RenderApi_GetFrameCounter();
        newCachedStagingBuffer.stagingBuffer = CreateStagingBuffer(count);

        return newCachedStagingBuffer.stagingBuffer;
    }

    GpuBufferHolderMemoryPool<StructType> m_pool;
};

} // namespace hyperion
