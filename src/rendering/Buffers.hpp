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
        return m_buffers[frameIndex];
    }

    virtual void MarkDirty(uint32 index) = 0;

    virtual void UpdateBufferSize(uint32 frameIndex) = 0;
    virtual void UpdateBufferData(uint32 frameIndex) = 0;

    /*! \brief Copy an element from the GPU back to the CPU side buffer.
     * \param frameIndex The index of the frame to copy the element from.
     * \param index The index of the element to copy.
     * \param dst The destination pointer to copy the element to.
     */
    virtual void ReadbackElement(uint32 frameIndex, uint32 index, void* dst) = 0;

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
    void CreateBuffers(GpuBufferType type, SizeType count, SizeType size, SizeType alignment = 0);

    virtual void WriteBufferData_Internal(uint32 index, const void* ptr) = 0;

    TypeId m_structTypeId;
    SizeType m_structSize;

    FixedArray<GpuBufferRef, g_framesInFlight> m_buffers;
    FixedArray<Range<uint32>, g_framesInFlight> m_dirtyRanges;
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

    HYP_FORCE_INLINE void MarkDirty(uint32 index)
    {
        Threads::AssertOnThread(g_renderThread);
        HYP_MT_CHECK_READ(m_dataRaceDetector);

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
        Threads::AssertOnThread(g_renderThread);
        HYP_MT_CHECK_READ(m_dataRaceDetector);

        bool wasResized = false;
        HYP_GFX_ASSERT(buffer->EnsureCapacity(Base::NumAllocatedElements() * sizeof(StructType), &wasResized));

        if (wasResized)
        {
            // Reset the dirty ranges
            for (auto& it : m_dirtyRanges)
            {
                it.SetStart(0);
                it.SetEnd(Base::NumAllocatedElements());
            }

            HYP_LOG_TEMP("RESIZE BUFFER {} to {} bytes",
                TypeNameHelper<StructType, true>::value.Data(),
                Base::NumAllocatedElements() * sizeof(StructType));
        }
    }

    void CopyToGpuBuffer(const GpuBufferRef& buffer, uint32 frameIndex)
    {
        Threads::AssertOnThread(g_renderThread);
        HYP_MT_CHECK_READ(m_dataRaceDetector);

        if (!m_dirtyRanges[frameIndex])
        {
            return;
        }

        const uint32 rangeEnd = m_dirtyRanges[frameIndex].GetEnd(),
                     rangeStart = m_dirtyRanges[frameIndex].GetStart();

        AssertDebug(buffer->Size() >= rangeEnd * sizeof(StructType),
            "Buffer does not have enough space for the current number of elements! Buffer size = %llu",
            buffer->Size());

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

            const SizeType offset = index;
            const SizeType count = Base::numElementsPerBlock;

            // sanity checks
            AssertDebug((offset - index) * sizeof(StructType) < sizeof(beginIt->buffer));
            AssertDebug(count <= int64(bufferSize / sizeof(StructType)) - int64(offset),
                "Buffer does not have enough space for the current number of elements! Buffer size = %llu, Required size = %llu",
                bufferSize,
                (offset + count) * sizeof(StructType));

            buffer->Copy(
                offset * sizeof(StructType),
                count * sizeof(StructType),
                &reinterpret_cast<StructType*>(beginIt->buffer.GetPointer())[offset - index]);
        }

        m_dirtyRanges[frameIndex].Reset();
    }

private:
    // @TODO Make atomic
    FixedArray<Range<uint32>, 2> m_dirtyRanges;

protected:
    HYP_DECLARE_MT_CHECK(m_dataRaceDetector);
};

template <class StructType, GpuBufferType BufferType>
class GpuBufferHolder final : public GpuBufferHolderBase
{
public:
    GpuBufferHolder(uint32 initialCount = 0)
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

    virtual void UpdateBufferSize(uint32 frameIndex) override
    {
        Threads::AssertOnThread(g_renderThread);
        HYP_MT_CHECK_READ(m_dataRaceDetector);

        // m_pool.RemoveEmptyBlocks();
        m_pool.EnsureGpuBufferCapacity(m_buffers[frameIndex], frameIndex);
    }

    virtual void UpdateBufferData(uint32 frameIndex) override
    {
        Threads::AssertOnThread(g_renderThread);
        HYP_MT_CHECK_READ(m_dataRaceDetector);

        m_pool.CopyToGpuBuffer(m_buffers[frameIndex], frameIndex);
    }

    virtual void MarkDirty(uint32 index) override
    {
        Threads::AssertOnThread(g_renderThread);
        HYP_MT_CHECK_READ(m_dataRaceDetector);

        m_pool.MarkDirty(index);
    }

    virtual void ReadbackElement(uint32 frameIndex, uint32 index, void* dst) override
    {
        Threads::AssertOnThread(g_renderThread);
        HYP_MT_CHECK_READ(m_dataRaceDetector);

        AssertDebug(index < m_pool.NumAllocatedElements(), "Index out of bounds! Index = {}, Size = {}", index, m_pool.NumAllocatedElements());

        m_buffers[frameIndex]->Read(sizeof(StructType) * index, sizeof(StructType), dst);
    }

    HYP_FORCE_INLINE uint32 AcquireIndex(StructType** outElementPtr)
    {
        Threads::AssertOnThread(g_renderThread);
        HYP_MT_CHECK_READ(m_dataRaceDetector);

        return m_pool.AcquireIndex(outElementPtr);
    }

    virtual uint32 AcquireIndex(void** outElementPtr = nullptr) override
    {
        Threads::AssertOnThread(g_renderThread);
        HYP_MT_CHECK_READ(m_dataRaceDetector);

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
        Threads::AssertOnThread(g_renderThread);
        HYP_MT_CHECK_READ(m_dataRaceDetector);

        return m_pool.ReleaseIndex(batchIndex);
    }

    virtual void EnsureCapacity(uint32 index) override
    {
        Threads::AssertOnThread(g_renderThread);
        HYP_MT_CHECK_READ(m_dataRaceDetector);

        m_pool.EnsureCapacity(index);
    }

    HYP_FORCE_INLINE void WriteBufferData(uint32 index, const StructType& value)
    {
        Threads::AssertOnThread(g_renderThread);
        HYP_MT_CHECK_READ(m_dataRaceDetector);

        m_pool.SetElement(index, value);
    }

    virtual void* GetCpuMapping(uint32 index) override
    {
        Threads::AssertOnThread(g_renderThread);
        HYP_MT_CHECK_READ(m_dataRaceDetector);

        AssertDebug(index < m_pool.NumAllocatedElements(), "Index out of bounds! Index = {}, Size = {}", index, m_pool.NumAllocatedElements());

        return &m_pool.GetElement(index);
    }

private:
    virtual void WriteBufferData_Internal(uint32 index, const void* bufferDataPtr) override
    {
        Threads::AssertOnThread(g_renderThread);
        HYP_MT_CHECK_READ(m_dataRaceDetector);

        WriteBufferData(index, *reinterpret_cast<const StructType*>(bufferDataPtr));
    }

    GpuBufferHolderMemoryPool<StructType> m_pool;
};

} // namespace hyperion
