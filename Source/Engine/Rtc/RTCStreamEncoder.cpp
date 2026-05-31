/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#include <HyperionPch.hpp>
#include <RTC/RTCStreamEncoder.hpp>
#include <RTC/RTCServer.hpp>
#include <RTC/RTCTrack.hpp>

#include <Core/Threading/AtomicVar.hpp>
#include <Core/Threading/TaskThread.hpp>

#include <Core/Logging/Logger.hpp>
#include <Core/Logging/LogChannels.hpp>

#include <mutex>

#ifdef HYP_GSTREAMER

#include <Core/FileSystem/FsUtil.hpp>

#include <gst/gst.h>
#include <gst/app/app.h>
#include <gst/gstsample.h>

#endif

#ifdef _WIN32
#include <winsock2.h>
#else
#include <arpa/inet.h>
#endif

namespace Hyperion {

class EncoderDataQueue
{
public:
    static constexpr uint32 maxQueueSize = 5;

    EncoderDataQueue() = default;
    EncoderDataQueue(const EncoderDataQueue& other) = delete;
    EncoderDataQueue& operator=(const EncoderDataQueue& other) = delete;
    EncoderDataQueue(EncoderDataQueue&& other) noexcept = delete;
    EncoderDataQueue& operator=(EncoderDataQueue&& other) noexcept = delete;
    ~EncoderDataQueue() = default;

    void Push(ByteBuffer data)
    {
        std::lock_guard guard(m_mutex);

        if constexpr (maxQueueSize != uint32(-1))
        {
            while (m_queue.Size() >= maxQueueSize)
            {
                m_queue.Pop();
                m_size.Decrement(1, MemoryOrder::RELAXED);
            }
        }

        m_size.Increment(1, MemoryOrder::RELAXED);

        m_queue.Push(std::move(data));
    }

    ByteBuffer Pop()
    {
        std::lock_guard guard(m_mutex);

        m_size.Decrement(1, MemoryOrder::RELAXED);

        return m_queue.Pop();
    }

    HYP_FORCE_INLINE uint32 Size() const
    {
        return m_size.Get(MemoryOrder::RELAXED);
    }

private:
    Queue<ByteBuffer> m_queue;
    AtomicVar<uint32> m_size;
    std::mutex m_mutex;
};

void NullRTCStreamVideoEncoder::Start()
{
}

void NullRTCStreamVideoEncoder::Stop()
{
}

void NullRTCStreamVideoEncoder::PushData(ByteBuffer data)
{
}

Optional<ByteBuffer> NullRTCStreamVideoEncoder::PullData()
{
    DebugLog(LogType::Warn, "PullData() used on NullRTCStreamVideoEncoder will return an empty dataset\n");

    return {};
}

#ifdef HYP_GSTREAMER

struct GStreamerUserData
{
    GstAppSrc* appsrc;
    GstAppSink* appsink;

    EncoderDataQueue* inQueue;
    EncoderDataQueue* outQueue;

    uint32 sourceId;
    GSourceFunc pushDataCallback;

    uint64 timestamp;
    double sampleDuration;
};

class GStreamerThread : public TaskThread
{
public:
    GStreamerThread()
        : TaskThread(ThreadId(NAME("GStreamerThread"))),
          m_inQueue(MakeUnique<EncoderDataQueue>()),
          m_outQueue(MakeUnique<EncoderDataQueue>())
    {
        gst_debug_set_default_threshold(GST_LEVEL_WARNING);
        gst_init(nullptr, nullptr);

        GstPlugin* plugin = gst_plugin_load_by_name("app");
        Assert(plugin != nullptr, "Failed to load 'app' plugin\n");
        g_object_unref(plugin);

        m_customData = GStreamerUserData {};
        m_customData.timestamp = 0;
        m_customData.sampleDuration = 1.0 / 60.0;

        m_customData.inQueue = m_inQueue.Get();
        m_customData.outQueue = m_outQueue.Get();

        m_customData.sourceId = 0;
        m_customData.pushDataCallback = [](void* userDataVp) -> gboolean
        {
            GStreamerUserData* userData = (GStreamerUserData*)userDataVp;
            Assert(userData != nullptr);

            GstAppSrc* appsrc = userData->appsrc;
            Assert(appsrc != nullptr);

            const bool isAppSrc = GST_IS_APP_SRC(appsrc);
            Assert(isAppSrc);

            EncoderDataQueue* inQueue = userData->inQueue;

            ByteBuffer bytes;

            if (inQueue->Size())
            {
                bytes = inQueue->Pop();
            }
            else
            {
                return TRUE;
            }

            Assert(bytes.Size() == 1080 * 720 * 4);

            const gsize dataSize = bytes.Size();

            GstBuffer* buffer = gst_buffer_new_allocate(NULL, dataSize, NULL);
            GST_BUFFER_PTS(buffer) = userData->timestamp;
            GST_BUFFER_DTS(buffer) = userData->timestamp;
            GST_BUFFER_DURATION(buffer) = GST_SECOND / 60;
            userData->timestamp += GST_BUFFER_DURATION(buffer);

            GstMapInfo mapInfo;

            if (gst_buffer_map(buffer, &mapInfo, GST_MAP_WRITE))
            {
                memcpy(mapInfo.data, bytes.Data(), dataSize);
                gst_buffer_unmap(buffer, &mapInfo);
            }
            else
            {
                DebugLog(LogType::Error, "Failed to write into GStreamer buffer .Size=%llu\n", dataSize);

                gst_buffer_unref(buffer);

                return FALSE;
            }

            Assert(buffer != nullptr);
            Assert(GST_IS_BUFFER(buffer));

            GstFlowReturn ret = gst_app_src_push_buffer(GST_APP_SRC(appsrc), buffer);

            if (ret != GST_FLOW_OK)
            {
                DebugLog(LogType::Error, "appsrc: push buffer error %s\n", gst_flow_get_name(ret));
            }

            return TRUE;
        };

        m_feedDataCallback = [](GstElement*, guint unusedSize, GStreamerUserData* userData)
        {
            if (userData->sourceId == 0)
            {
                userData->sourceId = g_idle_add((GSourceFunc)userData->pushDataCallback, userData);
            }
        };

        m_recvDataCallback = [](GstElement* appsink, GStreamerUserData* userData) -> GstFlowReturn
        {
            Assert(userData != nullptr);

            GstSample* sample = gst_app_sink_pull_sample(GST_APP_SINK(appsink));

            if (sample != nullptr)
            {
                EncoderDataQueue* outQueue = userData->outQueue;

                GstBuffer* buffer = gst_sample_get_buffer(sample);
                GstMapInfo mapInfo;

                if (!gst_buffer_map(buffer, &mapInfo, GST_MAP_READ))
                {
                    DebugLog(LogType::Error, "Failed to map GStreamer buffer for reading\n");

                    gst_sample_unref(sample);

                    return GST_FLOW_ERROR;
                }

                // DebugLog(LogType::Info, "Sample received from GStreamer, Size=%llu\n", mapInfo.size);

                outQueue->Push(ByteBuffer(mapInfo.size, mapInfo.data));

                gst_buffer_unmap(buffer, &mapInfo);

                gst_sample_unref(sample);

                return GST_FLOW_OK;
            }

            return GST_FLOW_ERROR;
        };
    }

    virtual ~GStreamerThread() override = default;

    virtual void Stop() override
    {
        TaskThread::Stop();

        if (m_loop)
        {
            g_main_loop_quit(m_loop);
            g_main_loop_unref(m_loop);

            m_loop = nullptr;
        }

        if (m_pipeline)
        {
            gst_element_set_state(GST_ELEMENT(m_pipeline), GST_STATE_NULL);
            gst_object_unref(m_pipeline);

            m_pipeline = nullptr;
        }
    }

    // This method is thread-safe
    void Push(ByteBuffer data)
    {
        m_inQueue->Push(std::move(data));
    }

    // This method is thread-safe
    Optional<ByteBuffer> Pull()
    {
        if (m_outQueue->Size())
        {
            return m_outQueue->Pop();
        }

        return {};
    }

protected:
    virtual void operator()() override
    {
        Assert(m_pipeline == nullptr);
        Assert(m_appsrc == nullptr);
        Assert(m_appsink == nullptr);
        Assert(m_loop == nullptr);

        GError* error = nullptr;

        m_pipeline = GST_PIPELINE(gst_pipeline_new(NULL));

        GstElement* convert = gst_element_factory_make("videoconvert", "convert");

        GstElement* encoder = gst_element_factory_make("x264enc", "encoder");
        g_object_set(G_OBJECT(encoder), "tune", 0x00000004, NULL); // ultrafast
        g_object_set(G_OBJECT(encoder), "speed-preset", 1, NULL);
        g_object_set(G_OBJECT(encoder), "bitrate", 35000, NULL);
        g_object_set(G_OBJECT(encoder), "key-int-max", 1, NULL);
        g_object_set(G_OBJECT(encoder), "b-adapt", TRUE, NULL);
        g_object_set(G_OBJECT(encoder), "bframes", 1, NULL);

        m_appsrc = GST_APP_SRC(gst_element_factory_make("appsrc", "source"));
        g_object_set(
            m_appsrc,
            "block", FALSE,
            NULL);
        gst_app_src_set_max_buffers(m_appsrc, 3);
        gst_app_src_set_leaky_type(m_appsrc, GST_APP_LEAKY_TYPE_UPSTREAM);
        g_object_set(
            m_appsrc,
            "caps",
            gst_caps_new_simple(
                "video/x-raw",
                "format", G_TYPE_STRING, "RGBA",
                "width", G_TYPE_INT, 1080,
                "height", G_TYPE_INT, 720,
                "framerate", GST_TYPE_FRACTION, 60, 1,
                NULL),
            NULL);
        g_signal_connect(m_appsrc, "need-data", G_CALLBACK(m_feedDataCallback), &m_customData);

        m_appsink = GST_APP_SINK(gst_element_factory_make("appsink", "sink"));
        g_object_set(m_appsink, "emit-signals", TRUE, NULL);
        g_object_set(
            m_appsink,
            "sync", FALSE,
            NULL);
        gst_app_sink_set_drop(m_appsink, true);
        gst_app_sink_set_max_buffers(m_appsink, 3);
        g_signal_connect(m_appsink, "new-sample", G_CALLBACK(m_recvDataCallback), &m_customData);
        g_object_set(G_OBJECT(m_appsink), "format", GST_FORMAT_TIME, "is-live", TRUE, "do-timestamp", TRUE, NULL);

        gst_bin_add_many(GST_BIN(m_pipeline), GST_ELEMENT(m_appsrc), convert, encoder, GST_ELEMENT(m_appsink), NULL);
        gst_element_link_many(GST_ELEMENT(m_appsrc), convert, encoder, GST_ELEMENT(m_appsink), NULL);

        m_customData.appsink = m_appsink;
        m_customData.appsrc = m_appsrc;

        gst_element_set_state(GST_ELEMENT(m_pipeline), GST_STATE_PLAYING);

        m_loop = g_main_loop_new(NULL, FALSE);
        g_main_loop_run(m_loop);
    }

    GstAppSrc* m_appsrc = nullptr;
    GstAppSink* m_appsink = nullptr;
    GstPipeline* m_pipeline = nullptr;
    GMainLoop* m_loop = nullptr;

    UniquePtr<EncoderDataQueue> m_inQueue;
    UniquePtr<EncoderDataQueue> m_outQueue;
    GStreamerUserData m_customData;

    void (*m_feedDataCallback)(GstElement*, guint32, GStreamerUserData*);
    GstFlowReturn (*m_recvDataCallback)(GstElement*, GStreamerUserData*);
};

GStreamerRTCStreamVideoEncoder::GStreamerRTCStreamVideoEncoder()
    : m_thread(MakeUnique<GStreamerThread>())
{
}

GStreamerRTCStreamVideoEncoder::~GStreamerRTCStreamVideoEncoder()
{
    // Direct call to the method to prevent virtual call in destructor
    GStreamerRTCStreamVideoEncoder::Stop();
}

void GStreamerRTCStreamVideoEncoder::Start()
{
    Assert(m_thread != nullptr);

    if (!m_thread->IsRunning())
    {
        m_thread->Start();
    }
}

void GStreamerRTCStreamVideoEncoder::Stop()
{
    if (m_thread)
    {
        if (m_thread->IsRunning())
        {
            m_thread->Stop();
        }

        if (m_thread->CanJoin())
        {
            m_thread->Join();
        }
    }
}

void GStreamerRTCStreamVideoEncoder::PushData(ByteBuffer data)
{
    if (!m_thread || !m_thread->IsRunning())
    {
        DebugLog(LogType::Warn, "PushData() called but GStreamer thread is not running\n");

        return;
    }

    m_thread->Push(std::move(data));
}

Optional<ByteBuffer> GStreamerRTCStreamVideoEncoder::PullData()
{
    if (!m_thread || !m_thread->IsRunning())
    {
        DebugLog(LogType::Warn, "PullData() called but GStreamer thread is not running\n");

        return {};
    }

    Optional<ByteBuffer> pullResult = m_thread->Pull();

    if (pullResult)
    {
        ByteBuffer& byteBuffer = pullResult.Get();
        Assert(byteBuffer.Size() >= 4);

        size_t i = 0;

        uint8 startCodeLength = 0;

        union
        {
            ubyte startCodeBytes[4];
            uint32 startCode;
        };

        byteBuffer.Read(i, 4, &startCodeBytes[0]);
        i += 4;

        ubyte nalHeader = 0x0;

        if (startCodeBytes[0] == 0x0 && startCodeBytes[1] == 0x0 && startCodeBytes[2] == 0x0 && startCodeBytes[3] == 0x1)
        {
            startCodeLength = 4;
            byteBuffer.Read(i, 1, &nalHeader);
            i += 1;
        }
        else if (startCodeBytes[0] == 0x0 && startCodeBytes[1] == 0x0 && startCodeBytes[2] == 0x1)
        {
            startCodeLength = 3;
            nalHeader = startCodeBytes[3];
            startCodeBytes[3] = 0x0;
        }
        else
        {
            Assert(
                false,
                "Invalid NAL header! Read bytes: {} {} {} {}",
                (uint32)startCodeBytes[0],
                (uint32)startCodeBytes[1],
                (uint32)startCodeBytes[2],
                (uint32)startCodeBytes[3]);
        }

        const uint32 nalRefIdc = (nalHeader >> 5) & 0x03;
        const uint32 nalUnitType = nalHeader & 0x1F;

        // DebugLog(LogType::Debug, "Start code bytes: %x %x %x %x\tNAL header: %x\tRef idc: %u\tUnit Type: %u\n",
        //     startCodeBytes[0], startCodeBytes[1], startCodeBytes[2], startCodeBytes[3],
        //     nalHeader,
        //     nalRefIdc,
        //     nalUnitType);

        return Optional<ByteBuffer>(std::move(byteBuffer));

        // return ByteBuffer(byteBuffer.Size() - i, byteBuffer.Data() + i);
    }

    return {};
}

#endif // HYP_GSTREAMER

#if 0

#ifdef HYP_GSTREAMER
NullRTCStream::NullRTCStream(RTCStreamType streamType)
    : RTCStream(RTC_STREAM_TYPE_VIDEO, MakeUnique<GStreamerRTCStreamVideoEncoder>())
{
}
#else
NullRTCStream::NullRTCStream(RTCStreamType streamType)
    : RTCStream(RTC_STREAM_TYPE_VIDEO, MakeUnique<NullRTCStreamVideoEncoder>())
{
}
#endif // HYP_GSTREAMER

#ifdef HYP_LIBDATACHANNEL

#ifdef HYP_GSTREAMER
LibDataChannelRTCStream::LibDataChannelRTCStream(RTCStreamType streamType)
    : RTCStream(RTC_STREAM_TYPE_VIDEO, MakeUnique<GStreamerRTCStreamVideoEncoder>())
{
}
#else
LibDataChannelRTCStream::LibDataChannelRTCStream(RTCStreamType streamType)
    : RTCStream(RTC_STREAM_TYPE_VIDEO, MakeUnique<NullRTCStreamVideoEncoder>())
{
}
#endif // HYP_GSTREAMER

#endif // HYP_LIBDATACHANNEL

#endif

} // namespace Hyperion
