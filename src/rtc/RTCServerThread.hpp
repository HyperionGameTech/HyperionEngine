#ifndef HYPERION_V2_RTC_SERVER_THREAD_HPP
#define HYPERION_V2_RTC_SERVER_THREAD_HPP

#include <core/Thread.hpp>
#include <core/Scheduler.hpp>

namespace hyperion::v2 {

class RTCServer;

class RTCServerThread final : public Thread<Scheduler<Task<void>>, RTCServer *>
{
public:
    RTCServerThread();
    virtual ~RTCServerThread() override = default;

    void Stop();

    /*! \brief Atomically load the boolean value indicating that this thread is actively running */
    bool IsRunning() const
        { return m_is_running.Get(MemoryOrder::RELAXED); }

private:
    virtual void operator()(RTCServer *) override;

    AtomicVar<bool> m_is_running;
};

} // namespace hyperion::v2

#endif // HYPERION_V2_RTC_SERVER_THREAD_HPP