/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_STREAMING_THREAD_HPP
#define HYPERION_STREAMING_THREAD_HPP

#include <core/threading/TaskThread.hpp>

#include <core/memory/RefCountedPtr.hpp>

namespace hyperion {

class StreamingThread final : public TaskThread
{
public:
    StreamingThread();
    virtual ~StreamingThread() override;
};

HYP_API void SetGlobalStreamingThread(const RC<StreamingThread>& streaming_thread);
HYP_API const RC<StreamingThread>& GetGlobalStreamingThread();

} // namespace hyperion

#endif