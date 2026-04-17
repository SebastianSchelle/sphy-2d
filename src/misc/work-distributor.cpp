#include "work-distributor.hpp"

namespace sthread
{

WorkDistributor::WorkDistributor() {};

WorkDistributor::~WorkDistributor()
{
    {
        std::lock_guard<std::mutex> lock(stateMutex);
        stopRequested = true;
        awake = true;
    }
    stateCv.notify_all();
    for (auto& t : threads)
    {
        if (t.joinable())
        {
            t.join();
        }
    }
}

void WorkDistributor::init(int numThreads)
{
    for (int i = 0; i < numThreads; i++)
    {
        workQueues.push_back(ConcurrentQueue<WorkFunction>());
        threads.push_back(std::thread(&WorkDistributor::run, this, i));
    }
}

void WorkDistributor::awaken()
{
    {
        std::lock_guard<std::mutex> lock(stateMutex);
        awake = true;
    }
    stateCv.notify_all();
}

void WorkDistributor::suspend()
{
    std::lock_guard<std::mutex> lock(stateMutex);
    awake = false;
}

void WorkDistributor::addWork(WorkFunction work, int preferredThreadId)
{
    if (workQueues.empty())
    {
        return;
    }

    int queueIdx = preferredThreadId;
    if (queueIdx < 0 || queueIdx >= static_cast<int>(workQueues.size()))
    {
        queueIdx = static_cast<int>(nextQueueIdx % workQueues.size());
        nextQueueIdx++;
    }

    workQueues[queueIdx].enqueue(std::move(work));
    pendingTasks.fetch_add(1, std::memory_order_release);
    stateCv.notify_one();
}

void WorkDistributor::run(int threadId)
{
    while (true)
    {
        WorkFunction work;
        if (workQueues[threadId].try_dequeue(work))
        {
            work();
            const size_t before =
                pendingTasks.fetch_sub(1, std::memory_order_acq_rel);
            if (before == 1)
            {
                std::lock_guard<std::mutex> g(stateMutex);
                stateCv.notify_all();
            }
            continue;
        }

        std::unique_lock<std::mutex> lock(stateMutex);
        stateCv.wait(
            lock,
            [this]()
            {
                return stopRequested
                       || (awake
                           && pendingTasks.load(std::memory_order_acquire) > 0);
            });
        if (stopRequested)
        {
            return;
        }
    }
}

void WorkDistributor::waitForEmptyQueues()
{
    std::unique_lock<std::mutex> lock(stateMutex);
    stateCv.wait(lock,
                 [this]()
                 { return pendingTasks.load(std::memory_order_acquire) == 0; });
}

size_t WorkDistributor::getThreadCount() const
{
    return threads.size();
}

}  // namespace sthread