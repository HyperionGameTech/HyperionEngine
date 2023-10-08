#include <rtc/RTCStreamEncoder.hpp>
#include <rtc/RTCServer.hpp>

#include <core/lib/AtomicVar.hpp>
#include <core/lib/Queue.hpp>

#include <TaskThread.hpp>

#include <mutex>

#ifdef HYP_GSTREAMER

#include <util/fs/FsUtil.hpp>

#include <gst/gst.h>
#include <gst/app/app.h>
#include <gst/gstsample.h>

#endif

namespace hyperion::v2 {

class EncoderDataQueue
{
public:
    EncoderDataQueue()                                              = default;
    EncoderDataQueue(const EncoderDataQueue &other) = delete;
    EncoderDataQueue &operator=(const EncoderDataQueue &other)      = delete;
    EncoderDataQueue(EncoderDataQueue &&other) noexcept             = delete;
    EncoderDataQueue &operator=(EncoderDataQueue &&other) noexcept  = delete;
    ~EncoderDataQueue()                                             = default;

    void Push(ByteBuffer data)
    {
        std::lock_guard guard(m_mutex);

        m_size.Increment(1, MemoryOrder::RELAXED);

        m_queue.Push(std::move(data));
    }

    ByteBuffer Pop()
    {
        std::lock_guard guard(m_mutex);

        m_size.Decrement(1, MemoryOrder::RELAXED);

        return m_queue.Pop();
    }

    HYP_FORCE_INLINE
    UInt Size() const
        { return m_size.Get(MemoryOrder::RELAXED); }

private:
    Queue<ByteBuffer>  m_queue;
    AtomicVar<UInt>    m_size;
    std::mutex         m_mutex;
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

    return { };
}

#ifdef HYP_GSTREAMER

class GStreamerThread : public TaskThread
{
public:
    GStreamerThread()
        : TaskThread(ThreadID::CreateDynamicThreadID(HYP_NAME(GStreamerThread))),
          m_in_queue(new EncoderDataQueue()),
          m_out_queue(new EncoderDataQueue())
    {
#ifdef HYP_GSTREAMER_BIN_DIR
        _putenv_s("GST_PLUGIN_PATH", HYP_GSTREAMER_BIN_DIR);
        _putenv_s("GST_REGISTRY", HYP_GSTREAMER_BIN_DIR);
        DebugLog(LogType::Debug, "GStreamer plugin path: %s\n", getenv("GST_PLUGIN_PATH"));
#endif

        gst_debug_set_default_threshold(GST_LEVEL_DEBUG);
        gst_init(nullptr, nullptr);

        GstPlugin *plugin = gst_plugin_load_by_name("app");
        AssertThrowMsg(plugin != nullptr, "Failed to load 'app' plugin\n");
        g_object_unref(plugin);

        m_feed_data_callback = [](GstElement *appsrc, guint unused_size, gpointer user_data)
        {
            AssertThrow(user_data != nullptr);

            GstBuffer *buffer = nullptr;

            EncoderDataQueue *in_queue = static_cast<EncoderDataQueue *>(user_data);

            if (in_queue->Size()) {
                ByteBuffer first = in_queue->Pop();

                const gsize data_size = first.Size();

                GstBuffer *buffer = gst_buffer_new_allocate(NULL, data_size, NULL);
                
                GstMapInfo map_info;

                if (gst_buffer_map(buffer, &map_info, GST_MAP_WRITE)) {
                    memcpy(map_info.data, first.Data(), data_size);
                    gst_buffer_unmap(buffer, &map_info);
                } else {
                    gst_buffer_unref(buffer);
                    buffer = nullptr;

                    DebugLog(LogType::Error, "Failed to write into GStreamer buffer .Size=%llu\n", data_size);
                }
            }

            if (buffer) {
                gst_app_src_push_buffer(GST_APP_SRC(appsrc), buffer);
            } else {
                // End of stream if no more data
                gst_app_src_end_of_stream(GST_APP_SRC(appsrc));
            }
        };

        m_recv_data_callback = [](GstElement *appsink, guint unused_size, gpointer user_data)
        {
            AssertThrow(user_data != nullptr);

            GstSample *sample = gst_app_sink_pull_sample(GST_APP_SINK(appsink));

            if (!sample) {
                return;
            }

            GstBuffer *buffer = gst_sample_get_buffer(sample);
            GstMapInfo map_info;

            if (!gst_buffer_map(buffer, &map_info, GST_MAP_READ)) {
                DebugLog(LogType::Error, "Failed to map GStreamer buffer for reading\n");

                gst_sample_unref(sample);

                return;
            }
            
            EncoderDataQueue *out_queue = static_cast<EncoderDataQueue *>(user_data);
            out_queue->Push(ByteBuffer(map_info.size, map_info.data));

            gst_sample_unref(sample);
        };
    }

    virtual ~GStreamerThread() override = default;

    virtual void Stop() override
    {
        TaskThread::Stop();

        if (m_loop) {
            g_main_loop_quit(m_loop);
            g_main_loop_unref(m_loop);

            m_loop = nullptr;
        }

        if (m_pipeline) {
            gst_element_set_state(m_pipeline, GST_STATE_NULL);
            gst_object_unref(GST_OBJECT(m_pipeline));

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
        if (m_out_queue->Size()) {
            return m_out_queue->Pop();
        }

        return { };
    }

protected:
    virtual void operator()() override
    {
        m_is_running.Set(true, MemoryOrder::RELAXED);

        AssertThrow(m_pipeline == nullptr);
        AssertThrow(m_appsrc == nullptr);
        AssertThrow(m_appsink == nullptr);
        AssertThrow(m_loop == nullptr);

        GError *error = nullptr;

        // Init Pipeline, AppSrc, AppSink
        m_pipeline = gst_parse_launch(
            "appsrc name=source ! video/x-raw,format=RGB,width=640,height=480 ! videoconvert ! x264enc pass=qual quantizer=20 tune=zerolatency ! h264parse ! appsink name=sink", &error
        );

        AssertThrowMsg(error == nullptr, "Failed to create GStreamer pipeline: %s\n", error->message);

        m_appsrc = gst_bin_get_by_name(GST_BIN(m_pipeline), "source");
        g_signal_connect(m_appsrc, "need-data", G_CALLBACK(m_feed_data_callback), m_in_queue.Get());

        m_appsink = gst_bin_get_by_name(GST_BIN(m_pipeline), "sink");
        g_signal_connect(m_appsink, "new-sample", G_CALLBACK(m_recv_data_callback), m_out_queue.Get());

        m_loop = g_main_loop_new(NULL, FALSE);

        g_main_loop_run(m_loop);
    }

    GstElement                      *m_appsrc = nullptr;
    GstElement                      *m_appsink = nullptr;
    GstElement                      *m_pipeline = nullptr;
    GMainLoop                       *m_loop = nullptr;

    UniquePtr<EncoderDataQueue>     m_in_queue;
    UniquePtr<EncoderDataQueue>     m_out_queue;
    void                            (*m_feed_data_callback)(GstElement *, guint, gpointer);
    void                            (*m_recv_data_callback)(GstElement *, guint, gpointer);
};

GStreamerRTCStreamVideoEncoder::GStreamerRTCStreamVideoEncoder()
    : m_thread(new GStreamerThread())
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

    if (!m_thread->IsRunning()) {
        m_thread->Start();
    }
}

void GStreamerRTCStreamVideoEncoder::Stop()
{
    if (m_thread) {
        if (m_thread->IsRunning()) {
            m_thread->Stop();
        }

        if (m_thread->CanJoin()) {
            m_thread->Join();
        }
    }
}

void GStreamerRTCStreamVideoEncoder::PushData(ByteBuffer data)
{
    if (!m_thread || !m_thread->IsRunning()) {
        DebugLog(LogType::Warn, "PushData() called but GStreamer thread is not running\n");

        return;
    }

    m_thread->Push(std::move(data));
}

Optional<ByteBuffer> GStreamerRTCStreamVideoEncoder::PullData()
{
    if (!m_thread || !m_thread->IsRunning()) {
        DebugLog(LogType::Warn, "PullData() called but GStreamer thread is not running\n");

        return { };
    }

    return m_thread->Pull();
}


#endif // HYP_GSTREAMER

#if 0

#ifdef HYP_GSTREAMER
NullRTCStream::NullRTCStream(RTCStreamType stream_type)
    : RTCStream(RTC_STREAM_TYPE_VIDEO, UniquePtr<RTCStreamEncoder>(new GStreamerRTCStreamVideoEncoder()))
{
}
#else
NullRTCStream::NullRTCStream(RTCStreamType stream_type)
    : RTCStream(RTC_STREAM_TYPE_VIDEO, UniquePtr<RTCStreamEncoder>(new NullRTCStreamVideoEncoder()))
{
}
#endif // HYP_GSTREAMER

#ifdef HYP_LIBDATACHANNEL

#ifdef HYP_GSTREAMER
LibDataChannelRTCStream::LibDataChannelRTCStream(RTCStreamType stream_type)
    : RTCStream(RTC_STREAM_TYPE_VIDEO, UniquePtr<RTCStreamEncoder>(new GStreamerRTCStreamVideoEncoder()))
{
}
#else
LibDataChannelRTCStream::LibDataChannelRTCStream(RTCStreamType stream_type)
    : RTCStream(RTC_STREAM_TYPE_VIDEO, UniquePtr<RTCStreamEncoder>(new NullRTCStreamVideoEncoder()))
{
}
#endif // HYP_GSTREAMER

#endif // HYP_LIBDATACHANNEL

#endif

}  // namespace hyperion::v2