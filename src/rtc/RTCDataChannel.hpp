#ifndef HYPERION_V2_RTC_DATA_CHANNEL_HPP
#define HYPERION_V2_RTC_DATA_CHANNEL_HPP

#include <core/lib/ByteBuffer.hpp>
#include <core/lib/String.hpp>

#ifdef HYP_LIBDATACHANNEL

namespace rtc {
class DataChannel;
} // namespace rtc

#include <memory>

class LibDataChannelRTCClient;

#endif

namespace hyperion::v2 {

class HYP_API RTCDataChannel
{
public:
    RTCDataChannel()                                            = default;
    RTCDataChannel(const RTCDataChannel &other)                 = delete;
    RTCDataChannel &operator=(const RTCDataChannel &other)      = delete;
    RTCDataChannel(RTCDataChannel &&other) noexcept             = delete;
    RTCDataChannel &operator=(RTCDataChannel &&other) noexcept  = delete;
    virtual ~RTCDataChannel()                                   = default;

    virtual void Send(const ByteBuffer &) = 0;
    void Send(const String &);
};

class HYP_API NullRTCDataChannel : public RTCDataChannel
{
public:
    using RTCDataChannel::Send;

    NullRTCDataChannel()                                                = default;
    NullRTCDataChannel(const NullRTCDataChannel &other)                 = delete;
    NullRTCDataChannel &operator=(const NullRTCDataChannel &other)      = delete;
    NullRTCDataChannel(NullRTCDataChannel &&other) noexcept             = delete;
    NullRTCDataChannel &operator=(NullRTCDataChannel &&other) noexcept  = delete;
    virtual ~NullRTCDataChannel() override                              = default;

    virtual void Send(const ByteBuffer &) override;
};

#ifdef HYP_LIBDATACHANNEL

class HYP_API LibDataChannelRTCDataChannel : public RTCDataChannel
{
public:
    friend class LibDataChannelRTCClient;

    using RTCDataChannel::Send;

    LibDataChannelRTCDataChannel()                                                          = default;
    LibDataChannelRTCDataChannel(const LibDataChannelRTCDataChannel &other)                 = delete;
    LibDataChannelRTCDataChannel &operator=(const LibDataChannelRTCDataChannel &other)      = delete;
    LibDataChannelRTCDataChannel(LibDataChannelRTCDataChannel &&other) noexcept             = delete;
    LibDataChannelRTCDataChannel &operator=(LibDataChannelRTCDataChannel &&other) noexcept  = delete;
    virtual ~LibDataChannelRTCDataChannel()                                                 = default;

    virtual void Send(const ByteBuffer &) override;

private:
    std::shared_ptr<rtc::DataChannel> m_data_channel;
};

#else

using LibDataChannelRTCDataChannel = NullRTCDataChannel;

#endif

} // namespace hyperion::v2

#endif