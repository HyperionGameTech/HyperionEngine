/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */
#include <rtc/RTCStreamEncoder.hpp>
#include <rtc/RTCServer.hpp>
#include <rtc/RTCTrack.hpp>

#include <core/threading/AtomicVar.hpp>
#include <core/threading/TaskThread.hpp>

#include <core/containers/Queue.hpp>

#include <mutex>

#ifdef HYP_GSTREAMER

    #include <core/filesystem/FsUtil.hpp>

    #include <gst/gst.h>
    #include <gst/app/app.h>
    #include <gst/gstsample.h>

#endif

#ifdef _WIN32
    #include <winsock2.h>
#else
    #include <arpa/inet.h>
#endif

namespace hyperion {

class EncoderDataQueue
{
public:
    static constexpr uint32 max_queue_size = 5;

    EncoderDataQueue() = default;
    EncoderDataQueue(const EncoderDataQueue& other) = delete;
    EncoderDataQueue& operator=(const EncoderDataQueue& other) = delete;
    EncoderDataQueue(EncoderDataQueue&& other) noexcept = delete;
    EncoderDataQueue& operator=(EncoderDataQueue&& other) noexcept = delete;
    ~EncoderDataQueue() = default;

    void Push(ByteBuffer data)
    {
        std::lock_guard guard(m_mutex);

        if constexpr (max_queue_size != uint32(-1))
        {
            while (m_queue.Size() >= max_queue_size)
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

    EncoderDataQueue* in_queue;
    EncoderDataQueue* out_queue;

    uint32 source_id;
    GSourceFunc push_data_callback;

    uint64 timestamp;
    double sample_duration;
};

class GStreamerThread : public TaskThread
{
public:
    GStreamerThread()
        : TaskThread(ThreadID(NAME("GStreamerThread"))),
          m_in_queue(MakeUnique<EncoderDataQueue>()),
          m_out_queue(MakeUnique<EncoderDataQueue>())
    {
        gst_debug_set_default_threshold(GST_LEVEL_WARNING);
        gst_init(nullptr, nullptr);

        GstPlugin* plugin = gst_plugin_load_by_name("app");
        AssertThrowMsg(plugin != nullptr, "Failed to load 'app' plugin\n");
        g_object_unref(plugin);

        m_custom_data = GStreamerUserData {};
        m_custom_data.timestamp = 0;
        m_custom_data.sample_duration = 1.0 / 60.0;

        m_custom_data.in_queue = m_in_queue.Get();
        m_custom_data.out_queue = m_out_queue.Get();

        m_custom_data.source_id = 0;
        m_custom_data.push_data_callback = [](void* user_data_vp) -> gboolean
        {
            GStreamerUserData* user_data = (GStreamerUserData*)user_data_vp;
            AssertThrow(user_data != nullptr);

            GstAppSrc* appsrc = user_data->appsrc;
            AssertThrow(appsrc != nullptr);
            AssertThrow(GST_IS_APP_SRC(appsrc));

            EncoderDataQueue* in_queue = user_data->in_queue;

            ByteBuffer bytes;

            if (in_queue->Size())
            {
                bytes = in_queue->Pop();
            }
            else
            {
                return TRUE;
            }

            AssertThrow(bytes.Size() == 1080 * 720 * 4);

            const gsize data_size = bytes.Size();

            GstBuffer* buffer = gst_buffer_new_allocate(NULL, data_size, NULL);
            GST_BUFFER_PTS(buffer) = user_data->timestamp;
            GST_BUFFER_DTS(buffer) = user_data->timestamp;
            GST_BUFFER_DURATION(buffer) = GST_SECOND / 60;
            user_data->timestamp += GST_BUFFER_DURATION(buffer);

            GstMapInfo map_info;

            if (gst_buffer_map(buffer, &map_info, GST_MAP_WRITE))
            {
                memcpy(map_info.data, bytes.Data(), data_size);
                gst_buffer_unmap(buffer, &map_info);
            }
            else
            {
                DebugLog(LogType::Error, "Failed to write into GStreamer buffer .Size=%llu\n", data_size);

                gst_buffer_unref(buffer);

                return FALSE;
            }

            AssertThrow(buffer != nullptr);
            AssertThrow(GST_IS_BUFFER(buffer));

            GstFlowReturn ret = gst_app_src_push_buffer(GST_APP_SRC(appsrc), buffer);

            if (ret != GST_FLOW_OK)
            {
                DebugLog(LogType::Error, "appsrc: push buffer error %s\n", gst_flow_get_name(ret));
            }

            return TRUE;
        };

        m_feed_data_callback = [](GstElement*, guint unused_size, GStreamerUserData* user_data)
        {
            if (user_data->source_id == 0)
            {
                user_data->source_id = g_idle_add((GSourceFunc)user_data->push_data_callback, user_data);
            }
        };

        m_recv_data_callback = [](GstElement* appsink, GStreamerUserData* user_data) -> GstFlowReturn
        {
            AssertThrow(user_data != nullptr);

            GstSample* sample = gst_app_sink_pull_sample(GST_APP_SINK(appsink));

            if (sample != nullptr)
            {
                EncoderDataQueue* out_queue = user_data->out_queue;

                GstBuffer* buffer = gst_sample_get_buffer(sample);
                GstMapInfo map_info;

                if (!gst_buffer_map(buffer, &map_info, GST_MAP_READ))
                {
                    DebugLog(LogType::Error, "Failed to map GStreamer buffer for reading\n");

                    gst_sample_unref(sample);

                    return GST_FLOW_ERROR;
                }

                // DebugLog(LogType::Info, "Sample received from GStreamer, Size=%llu\n", map_info.size);

                out_queue->Push(ByteBuffer(map_info.size, map_info.data));

                gst_buffer_unmap(buffer, &map_info);

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
        m_in_queue->Push(std::move(data));
    }

    // This method is thread-safe
    Optional<ByteBuffer> Pull()
    {
        if (m_out_queue->Size())
        {
            return m_out_queue->Pop();
        }

        return {};
    }

protected:
    virtual void operator()() override
    {
        m_is_running.Set(true, MemoryOrder::RELAXED);

        AssertThrow(m_pipeline == nullptr);
        AssertThrow(m_appsrc == nullptr);
        AssertThrow(m_appsink == nullptr);
        AssertThrow(m_loop == nullptr);

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
        g_signal_connect(m_appsrc, "need-data", G_CALLBACK(m_feed_data_callback), &m_custom_data);

        m_appsink = GST_APP_SINK(gst_element_factory_make("appsink", "sink"));
        g_object_set(m_appsink, "emit-signals", TRUE, NULL);
        g_object_set(
            m_appsink,
            "sync", FALSE,
            NULL);
        gst_app_sink_set_drop(m_appsink, true);
        gst_app_sink_set_max_buffers(m_appsink, 3);
        g_signal_connect(m_appsink, "new-sample", G_CALLBACK(m_recv_data_callback), &m_custom_data);
        g_object_set(G_OBJECT(m_appsink), "format", GST_FORMAT_TIME, "is-live", TRUE, "do-timestamp", TRUE, NULL);

        gst_bin_add_many(GST_BIN(m_pipeline), GST_ELEMENT(m_appsrc), convert, encoder, GST_ELEMENT(m_appsink), NULL);
        gst_element_link_many(GST_ELEMENT(m_appsrc), convert, encoder, GST_ELEMENT(m_appsink), NULL);

        m_custom_data.appsink = m_appsink;
        m_custom_data.appsrc = m_appsrc;

        gst_element_set_state(GST_ELEMENT(m_pipeline), GST_STATE_PLAYING);

        m_loop = g_main_loop_new(NULL, FALSE);
        g_main_loop_run(m_loop);
    }

    GstAppSrc* m_appsrc = nullptr;
    GstAppSink* m_appsink = nullptr;
    GstPipeline* m_pipeline = nullptr;
    GMainLoop* m_loop = nullptr;

    UniquePtr<EncoderDataQueue> m_in_queue;
    UniquePtr<EncoderDataQueue> m_out_queue;
    GStreamerUserData m_custom_data;

    void (*m_feed_data_callback)(GstElement*, guint32, GStreamerUserData*);
    GstFlowReturn (*m_recv_data_callback)(GstElement*, GStreamerUserData*);
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
    AssertThrow(m_thread != nullptr);

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

    Optional<ByteBuffer> pull_result = m_thread->Pull();

    if (pull_result)
    {
        ByteBuffer& byte_buffer = pull_result.Get();
        AssertThrow(byte_buffer.Size() >= 4);

        SizeType i = 0;

        uint8 start_code_length = 0;

        union
        {
            ubyte start_code_bytes[4];
            uint32 start_code;
        };

        byte_buffer.Read(i, 4, &start_code_bytes[0]);
        i += 4;

        ubyte nal_header = 0x0;

        if (start_code_bytes[0] == 0x0 && start_code_bytes[1] == 0x0 && start_code_bytes[2] == 0x0 && start_code_bytes[3] == 0x1)
        {
            start_code_length = 4;
            byte_buffer.Read(i, 1, &nal_header);
            i += 1;
        }
        else if (start_code_bytes[0] == 0x0 && start_code_bytes[1] == 0x0 && start_code_bytes[2] == 0x1)
        {
            start_code_length = 3;
            nal_header = start_code_bytes[3];
            start_code_bytes[3] = 0x0;
        }
        else
        {
            AssertThrowMsg(
                false,
                "Invalid NAL header! Read bytes: %x %x %x %x",
                start_code_bytes[0],
                start_code_bytes[1],
                start_code_bytes[2],
                start_code_bytes[3]);
        }

        const uint32 nal_ref_idc = (nal_header >> 5) & 0x03;
        const uint32 nal_unit_type = nal_header & 0x1F;

        // DebugLog(LogType::Debug, "Start code bytes: %x %x %x %x\tNAL header: %x\tRef idc: %u\tUnit Type: %u\n",
        //     start_code_bytes[0], start_code_bytes[1], start_code_bytes[2], start_code_bytes[3],
        //     nal_header,
        //     nal_ref_idc,
        //     nal_unit_type);

        return Optional<ByteBuffer>(std::move(byte_buffer));

        // return ByteBuffer(byte_buffer.Size() - i, byte_buffer.Data() + i);
    }

    return {};
}

#endif // HYP_GSTREAMER

#if 0

    #ifdef HYP_GSTREAMER
NullRTCStream::NullRTCStream(RTCStreamType stream_type)
    : RTCStream(RTC_STREAM_TYPE_VIDEO, MakeUnique<GStreamerRTCStreamVideoEncoder>())
{
}
    #else
NullRTCStream::NullRTCStream(RTCStreamType stream_type)
    : RTCStream(RTC_STREAM_TYPE_VIDEO, MakeUnique<NullRTCStreamVideoEncoder>())
{
}
    #endif // HYP_GSTREAMER

    #ifdef HYP_LIBDATACHANNEL

        #ifdef HYP_GSTREAMER
LibDataChannelRTCStream::LibDataChannelRTCStream(RTCStreamType stream_type)
    : RTCStream(RTC_STREAM_TYPE_VIDEO, MakeUnique<GStreamerRTCStreamVideoEncoder>())
{
}
        #else
LibDataChannelRTCStream::LibDataChannelRTCStream(RTCStreamType stream_type)
    : RTCStream(RTC_STREAM_TYPE_VIDEO, MakeUnique<NullRTCStreamVideoEncoder>())
{
}
        #endif // HYP_GSTREAMER

    #endif // HYP_LIBDATACHANNEL

#endif

} // namespace hyperion