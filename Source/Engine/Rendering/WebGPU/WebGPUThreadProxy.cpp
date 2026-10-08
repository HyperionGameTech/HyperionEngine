/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#include <WebGPUPch.hpp>

#include <Rendering/WebGPU/WebGPUShared.hpp>

#ifdef HYP_WEB

#include <emscripten/proxying.h>
#include <emscripten/threading.h>

#include <atomic>
#include <climits>

namespace Hyperion {

pthread_t g_webGPUDeviceThread {};

// What the device thread is blocked on, if it is blocked rather than in its event loop
static std::atomic<void*> g_deviceThreadWaitAddress { nullptr };

struct DeviceThreadCall
{
    void (*function)(void*);
    void* arg;
    std::atomic<uint32_t> isDone;
};

void RunOnWebGPUDeviceThread(void (*function)(void*), void* arg)
{
    DeviceThreadCall call { function, arg, { 0 } };

    emscripten_proxy_async(
        emscripten_proxy_get_system_queue(), g_webGPUDeviceThread,
        [](void* callPtr)
        {
            DeviceThreadCall* call = static_cast<DeviceThreadCall*>(callPtr);
            call->function(call->arg);

            call->isDone.store(1, std::memory_order_release);
            emscripten_futex_wake(&call->isDone, 1);
        },
        &call);

    while (call.isDone.load(std::memory_order_acquire) == 0)
    {
        // The queue is run from the event loop; a device thread blocked on a lock or a signal has to be woken to look at it.
        if (void* waitAddress = g_deviceThreadWaitAddress.load(std::memory_order_acquire))
        {
            emscripten_futex_wake(waitAddress, INT_MAX);
        }

        emscripten_futex_wait(&call.isDone, 0, 1.0);
    }
}

} // namespace Hyperion

extern "C" {

int __real_emscripten_futex_wait(volatile void* address, uint32_t value, double maxWaitMilliseconds);

// Linked with --wrap: every blocking wait on the device thread first runs the calls other threads have handed it,
// otherwise it would deadlock waiting on a thread that is itself waiting for one of those calls.
int __wrap_emscripten_futex_wait(volatile void* address, uint32_t value, double maxWaitMilliseconds)
{
    using namespace Hyperion;

    static thread_local bool isRunningQueue = false;

    if (isRunningQueue || g_webGPUDeviceThread == pthread_t {} || !pthread_equal(pthread_self(), g_webGPUDeviceThread))
    {
        return __real_emscripten_futex_wait(address, value, maxWaitMilliseconds);
    }

    g_deviceThreadWaitAddress.store(const_cast<void*>(address), std::memory_order_release);

    isRunningQueue = true;
    emscripten_current_thread_process_queued_calls();
    isRunningQueue = false;

    // a wake sent for the queue looks like a spurious wake to the caller, which every futex user has to handle anyway
    const int result = __real_emscripten_futex_wait(address, value, maxWaitMilliseconds);

    g_deviceThreadWaitAddress.store(nullptr, std::memory_order_release);

    isRunningQueue = true;
    emscripten_current_thread_process_queued_calls();
    isRunningQueue = false;

    return result;
}

} // extern "C"

#endif // HYP_WEB
