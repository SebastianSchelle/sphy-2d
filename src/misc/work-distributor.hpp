#ifndef WORK_DISTRIBUTOR_HPP
#define WORK_DISTRIBUTOR_HPP

#include <concurrentqueue.h>
#include <atomic>
#include <condition_variable>
#include <functional>
#include <mutex>
#include <thread>
#include <vector>

using moodycamel::ConcurrentQueue;

namespace misc
{

typedef std::function<void()> WorkFunction;

class WorkDistributor
{
  public:
    WorkDistributor();
    ~WorkDistributor();
    void init(int numThreads);
    void awaken();
    void suspend();
    void addWork(WorkFunction work, int preferredThreadId = -1);
  private:
    void run(int threadId);

    std::vector<std::thread> threads;
    std::vector<ConcurrentQueue<WorkFunction>> workQueues;
    mutable std::mutex stateMutex;
    std::condition_variable stateCv;
    bool awake = true;
    bool stopRequested = false;
    size_t nextQueueIdx = 0;
    std::atomic<size_t> pendingTasks{0};
};
}  // namespace misc

#endif