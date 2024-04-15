/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_V2_RTC_STREAM_ENCODER_HPP
#define HYPERION_V2_RTC_STREAM_ENCODER_HPP

#include <core/lib/String.hpp>
#include <core/lib/ByteBuffer.hpp>
#include <core/lib/UniquePtr.hpp>
#include <core/lib/Optional.hpp>

namespace hyperion {
class TaskThread;
} // namespace hyperion

namespace hyperion::v2 {

class RTCTrack;

class HYP_API RTCStreamEncoder
{
public:
    RTCStreamEncoder()                                                = default;
    RTCStreamEncoder(const RTCStreamEncoder &other)                   = delete;
    RTCStreamEncoder &operator=(const RTCStreamEncoder &other)        = delete;
    RTCStreamEncoder(RTCStreamEncoder &&other) noexcept               = default;
    RTCStreamEncoder &operator=(RTCStreamEncoder &&other) noexcept    = default;
    virtual ~RTCStreamEncoder()                                       = default;

    virtual void PushData(ByteBuffer data) = 0;
    virtual Optional<ByteBuffer> PullData() = 0;

    virtual void Start() = 0;
    virtual void Stop() = 0;
};

class HYP_API RTCStreamVideoEncoder : public RTCStreamEncoder
{
public:
    RTCStreamVideoEncoder()                                                     = default;
    RTCStreamVideoEncoder(const RTCStreamVideoEncoder &other)                   = delete;
    RTCStreamVideoEncoder &operator=(const RTCStreamVideoEncoder &other)        = delete;
    RTCStreamVideoEncoder(RTCStreamVideoEncoder &&other) noexcept               = default;
    RTCStreamVideoEncoder &operator=(RTCStreamVideoEncoder &&other) noexcept    = default;
    virtual ~RTCStreamVideoEncoder() override                                   = default;

    virtual void PushData(ByteBuffer data) override = 0;
    virtual Optional<ByteBuffer> PullData() override = 0;

    virtual void Start() override = 0;
    virtual void Stop() override = 0;
};

class HYP_API NullRTCStreamVideoEncoder : public RTCStreamVideoEncoder
{
public:
    NullRTCStreamVideoEncoder()                                                         = default;
    NullRTCStreamVideoEncoder(const NullRTCStreamVideoEncoder &other)                   = delete;
    NullRTCStreamVideoEncoder &operator=(const NullRTCStreamVideoEncoder &other)        = delete;
    NullRTCStreamVideoEncoder(NullRTCStreamVideoEncoder &&other) noexcept               = default;
    NullRTCStreamVideoEncoder &operator=(NullRTCStreamVideoEncoder &&other) noexcept    = default;
    virtual ~NullRTCStreamVideoEncoder() override                                       = default;

    virtual void PushData(ByteBuffer data) override;
    virtual Optional<ByteBuffer> PullData() override;

    virtual void Start() override;
    virtual void Stop() override;
};

#ifdef HYP_GSTREAMER

class GStreamerThread;

class HYP_API GStreamerRTCStreamVideoEncoder : public RTCStreamVideoEncoder
{
public:
    GStreamerRTCStreamVideoEncoder();
    GStreamerRTCStreamVideoEncoder(const GStreamerRTCStreamVideoEncoder &other)                 = delete;
    GStreamerRTCStreamVideoEncoder &operator=(const GStreamerRTCStreamVideoEncoder &other)      = delete;
    GStreamerRTCStreamVideoEncoder(GStreamerRTCStreamVideoEncoder &&other) noexcept             = default;
    GStreamerRTCStreamVideoEncoder &operator=(GStreamerRTCStreamVideoEncoder &&other) noexcept  = default;
    virtual ~GStreamerRTCStreamVideoEncoder() override;

    virtual void PushData(ByteBuffer data) override;
    virtual Optional<ByteBuffer> PullData() override;

    virtual void Start() override;
    virtual void Stop() override;

private:
    UniquePtr<GStreamerThread>  m_thread;
};

#else

using GStreamerRTCStreamVideoEncoder = NullRTCStreamVideoEncoder;

#endif

} // namespace hyperion::v2

#endif