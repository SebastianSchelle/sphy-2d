#ifndef _WIN32

#include "process.hpp"

#include <cerrno>
#include <csignal>
#include <cstring>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

namespace osh
{

void CloseFd(int& fd)
{
    if (fd >= 0)
    {
        ::close(fd);
        fd = -1;
    }
}

}  // namespace osh

namespace osh
{

Process::~Process()
{
    if (IsRunning())
        Terminate();

    if (stdout_thread_.joinable())
        stdout_thread_.join();

    if (stderr_thread_.joinable())
        stderr_thread_.join();

    if (pid_ > 0)
    {
        int status = 0;

        // Make sure we don't leave a zombie if something unusual
        // happened during destruction.
        ::waitpid(pid_, &status, 0);

        pid_ = -1;
    }

    CloseFd(stdout_fd_);
    CloseFd(stderr_fd_);
}


bool Process::Start(const std::string& executable,
                    const std::vector<std::string>& args)
{
    Options options;
    return Start(executable, args, options);
}

bool Process::Start(const std::string& executable,
                    const std::vector<std::string>& args,
                    const Options& options)
{
    std::lock_guard lock(mutex_);

    if (state_ == State::Running)
        return false;

    stdout_stream_ = options.stdout_stream;
    stderr_stream_ = options.stderr_stream;

    int stdout_pipe[2];
    int stderr_pipe[2];

    if (::pipe(stdout_pipe) != 0)
        return false;

    if (::pipe(stderr_pipe) != 0)
    {
        ::close(stdout_pipe[0]);
        ::close(stdout_pipe[1]);
        return false;
    }

    pid_t pid = ::fork();

    if (pid < 0)
    {
        ::close(stdout_pipe[0]);
        ::close(stdout_pipe[1]);
        ::close(stderr_pipe[0]);
        ::close(stderr_pipe[1]);

        state_ = State::Failed;
        return false;
    }

    if (pid == 0)
    {
        // Child process.

        ::close(stdout_pipe[0]);
        ::close(stderr_pipe[0]);

        if (::dup2(stdout_pipe[1], STDOUT_FILENO) < 0)
            _exit(127);

        if (::dup2(stderr_pipe[1], STDERR_FILENO) < 0)
            _exit(127);

        ::close(stdout_pipe[1]);
        ::close(stderr_pipe[1]);

        // Build argv.
        std::vector<char*> argv;
        argv.reserve(args.size() + 2);

        argv.push_back(const_cast<char*>(executable.c_str()));

        for (const auto& arg : args)
            argv.push_back(const_cast<char*>(arg.c_str()));

        argv.push_back(nullptr);

        // No shell involved.
        ::execvp(executable.c_str(), argv.data());

        // exec failed.
        _exit(127);
    }

    // Parent.

    ::close(stdout_pipe[1]);
    ::close(stderr_pipe[1]);

    pid_ = pid;
    stdout_fd_ = stdout_pipe[0];
    stderr_fd_ = stderr_pipe[0];

    state_ = State::Running;
    exit_code_ = -1;

    stdout_thread_ = std::thread(&Process::StdoutThread, this);
    stderr_thread_ = std::thread(&Process::StderrThread, this);

    return true;
}

void Process::StdoutThread()
{
    char buffer[4096];

    while (true)
    {
        ssize_t n = ::read(stdout_fd_, buffer, sizeof(buffer));

        if (n <= 0)
            break;

        if (stdout_stream_)
        {
            std::lock_guard lock(mutex_);

            stdout_stream_->write(buffer, n);
            stdout_stream_->flush();
        }
    }

    CloseFd(stdout_fd_);
}

void Process::StderrThread()
{
    char buffer[4096];

    while (true)
    {
        ssize_t n = ::read(stderr_fd_, buffer, sizeof(buffer));

        if (n <= 0)
            break;

        if (stderr_stream_)
        {
            std::lock_guard lock(mutex_);

            stderr_stream_->write(buffer, n);
            stderr_stream_->flush();
        }
    }

    CloseFd(stderr_fd_);
}

bool Process::IsRunningLocked() const
{
    if (pid_ <= 0)
        return false;

    int status = 0;

    pid_t result = ::waitpid(pid_, &status, WNOHANG);

    if (result == 0)
        return true;

    return false;
}

void Process::UpdateState()
{
    std::lock_guard lock(mutex_);

    if (state_ != State::Running || pid_ <= 0)
        return;

    int status = 0;

    pid_t result = ::waitpid(pid_, &status, WNOHANG);

    if (result == 0)
        return;

    if (result < 0)
    {
        if (errno == ECHILD)
        {
            state_ = State::Failed;
        }

        return;
    }

    if (WIFEXITED(status))
    {
        exit_code_ = WEXITSTATUS(status);
        state_ = State::Exited;
    }
    else if (WIFSIGNALED(status))
    {
        // Conventional representation for signal termination.
        exit_code_ = 128 + WTERMSIG(status);
        state_ = State::Exited;
    }
    else
    {
        state_ = State::Exited;
    }
}

Process::State Process::GetState()
{
    UpdateState();
    return state_.load();
}

bool Process::IsRunning()
{
    return GetState() == State::Running;
}

bool Process::HasExited()
{
    State state = GetState();

    return state == State::Exited || state == State::Failed;
}

bool Process::Terminate()
{
    std::lock_guard lock(mutex_);

    if (state_ != State::Running || pid_ <= 0)
        return false;

    return ::kill(pid_, SIGTERM) == 0;
}

int Process::Wait()
{
    {
        std::lock_guard lock(mutex_);

        if (state_ == State::NotStarted)
            return -1;

        if (state_ == State::Exited)
            return exit_code_;

        if (state_ == State::Failed)
            return -1;
    }

    int status = 0;

    if (::waitpid(pid_, &status, 0) < 0)
    {
        state_ = State::Failed;
        return -1;
    }

    if (WIFEXITED(status))
    {
        exit_code_ = WEXITSTATUS(status);
    }
    else if (WIFSIGNALED(status))
    {
        exit_code_ = 128 + WTERMSIG(status);
    }
    else
    {
        exit_code_ = -1;
    }

    state_ = State::Exited;

    if (stdout_thread_.joinable())
        stdout_thread_.join();

    if (stderr_thread_.joinable())
        stderr_thread_.join();

    return exit_code_;
}

std::uint64_t Process::ProcessId() const
{
    std::lock_guard lock(mutex_);

    return pid_ > 0 ? static_cast<std::uint64_t>(pid_) : 0;
}

int Process::ExitCode() const
{
    return exit_code_.load();
}

}  // namespace osh

#endif