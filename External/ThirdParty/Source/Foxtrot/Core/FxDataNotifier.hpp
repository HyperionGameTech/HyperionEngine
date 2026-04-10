#pragma once

#include <condition_variable>
#include <mutex>

class FxDataNotifier
{
public:
    FxDataNotifier() = default;

    void SignalDataWritten()
    {
        std::unique_lock<std::mutex> lock(mMutex);

        if (mKilled) {
            return;
        }

        mDone = true;

        // Notify the other thread
        mCV.notify_one();
    }

    void WaitForData(bool pass_if_already_done = false)
    {
        std::unique_lock<std::mutex> lock(mMutex);

        if (pass_if_already_done && mDone) {
            return;
        }

        mCV.wait(lock, [this] { return mDone; });
    }

    bool IsDone()
    {
        std::unique_lock<std::mutex> lock(mMutex);
        return mDone;
    }

    void Reset()
    {
        std::unique_lock<std::mutex> lock(mMutex);

        mKilled = false;
        mDone = false;
    }

    bool IsKilled()
    {
        std::unique_lock<std::mutex> lock(mMutex);

        return mKilled;
    }

    void Kill()
    {
        std::unique_lock<std::mutex> lock(mMutex);

        mKilled = true;
        mDone = true;

        mCV.notify_one();
    }


private:
    bool mDone = false;
    bool mKilled = false;

    std::condition_variable mCV;
    std::mutex mMutex;
};
