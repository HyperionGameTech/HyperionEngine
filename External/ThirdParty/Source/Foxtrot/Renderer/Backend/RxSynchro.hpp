#pragma once

#include <vulkan/vulkan.h>

#include <Core/FxPanic.hpp>
#include <Core/FxTypes.hpp>

#include <Core/FxBitset.hpp>

/**
 * @brief A CPU-GPU barrier.
 */
class RxFence
{
public:
    void Create();

    void WaitFor(uint64 timeout = UINT64_MAX) const;
    void Reset();

    FX_FORCE_INLINE VkFence Get() const { return Fence; }

    void Destroy();

public:
    VkFence Fence = nullptr;
};


/**
 * @brief A GPU-side barrier.
 */
class RxSemaphore
{
public:
    RxSemaphore() = default;

    void Create();

    FX_FORCE_INLINE VkSemaphore Get() const { return Semaphore; }

    void SetCacheId(uint32 id)
    {
        FxAssertMsg(mCacheId == UINT32_MAX, "Semaphore is already assigned to a cache slot!");
        mCacheId = id;
    }

    FX_FORCE_INLINE uint32 GetCacheId() const { return mCacheId; }
    void InvalidateCacheId() { mCacheId = UINT32_MAX; }

    void Destroy();

public:
    VkSemaphore Semaphore = nullptr;

private:
    uint32 mCacheId = UINT32_MAX;
};



class RxSemaphoreCache
{
    static constexpr uint32 scNumSemaphores = 24;

public:
    RxSemaphoreCache();

    RxSemaphore* Request();
    void Release(RxSemaphore* semaphore);

    ~RxSemaphoreCache();

private:
    FxSizedArray<RxSemaphore> mSemaphores;
    FxBitset mInUse;
};