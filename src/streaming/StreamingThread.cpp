/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <streaming/StreamingThread.hpp>

#include <core/threading/Mutex.hpp>

namespace hyperion {

#pragma region StreamingThread

static RC<StreamingThread> g_global_streaming_thread;
static Mutex g_global_streaming_thread_mutex;

HYP_API void SetGlobalStreamingThread(const RC<StreamingThread> &streaming_thread)
{
    Mutex::Guard guard(g_global_streaming_thread_mutex);

    g_global_streaming_thread = streaming_thread;
}

HYP_API const RC<StreamingThread> &GetGlobalStreamingThread()
{
    Mutex::Guard guard(g_global_streaming_thread_mutex);

    return g_global_streaming_thread;
}

StreamingThread::StreamingThread()
    : TaskThread(NAME("StreamingThread"), ThreadPriorityValue::LOW)
{
}

StreamingThread::~StreamingThread() = default;

#pragma endregion StreamingThread

} // namespace hyperion