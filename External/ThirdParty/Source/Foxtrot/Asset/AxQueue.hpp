#pragma once

#include "AxQueueItem.hpp"

#include <deque>
#include <mutex>

class AxQueue
{
public:
    AxQueue() = default;

    void Push(AxQueueItem&& value)
    {
        std::lock_guard<std::mutex> lock(mMutex);

        mQueue.push_back(std::move(value));
    }

    bool PopIfAvailable(AxQueueItem* item)
    {
        std::lock_guard<std::mutex> lock(mMutex);

        if (mQueue.empty()) {
            return false;
        }

        AxQueueItem& queue_item = mQueue.front();
        (*item) = std::move(queue_item);

        mQueue.pop_front();

        return true;
    }

    void Destroy()
    {
        std::lock_guard<std::mutex> lock(mMutex);

        mQueue.clear();
    }

private:
    std::deque<AxQueueItem> mQueue;

    std::mutex mMutex;
};
