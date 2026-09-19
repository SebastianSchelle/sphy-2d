#ifndef PROCESS_HPP
#define PROCESS_HPP


#include <atomic>
#include <cstdint>
#include <iostream>
#include <memory>
#include <mutex>
#include <ostream>
#include <string>
#include <thread>
#include <vector>

namespace osh
{

class Process
{
  public:
    enum class State
    {
        NotStarted,
        Running,
        Exited,
        Failed
    };

    struct Options
    {
        std::ostream* stdout_stream = &std::cout;
        std::ostream* stderr_stream = &std::cerr;
    };

  public:
    Process() = default;
    ~Process();

    Process(const Process&) = delete;
    Process& operator=(const Process&) = delete;

    Process(Process&&) = delete;
    Process& operator=(Process&&) = delete;

    // Starts:
    //
    //   executable arg1 arg2 ...
    //
    // Example:
    //   process.Start("my_program", {"--foo", "bar"});
    //
    // Returns false if the process could not be started.
    bool Start(const std::string& executable,
               const std::vector<std::string>& args);

    bool Start(const std::string& executable,
               const std::vector<std::string>& args,
               const Options& options);

    // Requests termination of the child.
    //
    // On POSIX this sends SIGTERM.
    // On Windows this calls TerminateProcess().
    //
    // Returns false if the process isn't running or termination failed.
    bool Terminate();

    // Waits until the process has exited.
    //
    // Returns the process exit code.
    int Wait();

    // Returns the current state.
    State GetState();

    bool IsRunning();
    bool HasExited();

    // Returns the OS process ID.
    //
    // Returns 0 if the process hasn't been started.
    std::uint64_t ProcessId() const;

    // Returns the exit code.
    //
    // Only meaningful after HasExited() == true.
    int ExitCode() const;

  private:
    void StdoutThread();
    void StderrThread();

    void UpdateState();

    bool IsRunningLocked() const;

  private:
#ifdef _WIN32
    void* process_handle_ = nullptr;
    void* stdout_read_ = nullptr;
    void* stderr_read_ = nullptr;
    std::uint64_t process_id_ = 0;
#else
    int pid_ = -1;
    int stdout_fd_ = -1;
    int stderr_fd_ = -1;
#endif

    std::atomic<State> state_{State::NotStarted};
    std::atomic<int> exit_code_{-1};

    std::ostream* stdout_stream_ = nullptr;
    std::ostream* stderr_stream_ = nullptr;

    std::thread stdout_thread_;
    std::thread stderr_thread_;

    mutable std::mutex mutex_;
};

}  // namespace osh

#endif