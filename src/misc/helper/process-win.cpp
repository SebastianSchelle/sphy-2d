#ifdef _WIN32

#include "process.hpp"

#define NOMINMAX
#include <windows.h>

#include <string>
#include <vector>

namespace osh
{

std::string QuoteWindowsArgument(const std::string& arg)
{
    if (arg.empty())
        return "\"\"";

    bool needs_quotes = false;

    for (char c : arg)
    {
        if (c == ' ' || c == '\t' || c == '"')
        {
            needs_quotes = true;
            break;
        }
    }

    if (!needs_quotes)
        return arg;

    std::string result = "\"";

    unsigned backslashes = 0;

    for (char c : arg)
    {
        if (c == '\\')
        {
            ++backslashes;
            continue;
        }

        if (c == '"')
        {
            // Backslashes before a quote need to be doubled.
            result.append(backslashes * 2 + 1, '\\');
            result += '"';
            backslashes = 0;
            continue;
        }

        result.append(backslashes, '\\');
        backslashes = 0;
        result += c;
    }

    // Backslashes before the closing quote must be doubled.
    result.append(backslashes * 2, '\\');
    result += '"';

    return result;
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

    if (process_handle_)
    {
        CloseHandle(static_cast<HANDLE>(process_handle_));
        process_handle_ = nullptr;
    }

    if (stdout_read_)
    {
        CloseHandle(static_cast<HANDLE>(stdout_read_));
        stdout_read_ = nullptr;
    }

    if (stderr_read_)
    {
        CloseHandle(static_cast<HANDLE>(stderr_read_));
        stderr_read_ = nullptr;
    }
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

    SECURITY_ATTRIBUTES sa{};
    sa.nLength = sizeof(sa);
    sa.bInheritHandle = TRUE;

    HANDLE stdout_read = nullptr;
    HANDLE stdout_write = nullptr;

    HANDLE stderr_read = nullptr;
    HANDLE stderr_write = nullptr;

    if (!CreatePipe(&stdout_read, &stdout_write, &sa, 0))
    {
        state_ = State::Failed;
        return false;
    }

    if (!SetHandleInformation(stdout_read, HANDLE_FLAG_INHERIT, 0))
    {
        CloseHandle(stdout_read);
        CloseHandle(stdout_write);

        state_ = State::Failed;
        return false;
    }

    if (!CreatePipe(&stderr_read, &stderr_write, &sa, 0))
    {
        CloseHandle(stdout_read);
        CloseHandle(stdout_write);

        state_ = State::Failed;
        return false;
    }

    if (!SetHandleInformation(stderr_read, HANDLE_FLAG_INHERIT, 0))
    {
        CloseHandle(stdout_read);
        CloseHandle(stdout_write);
        CloseHandle(stderr_read);
        CloseHandle(stderr_write);

        state_ = State::Failed;
        return false;
    }

    // Windows CreateProcess takes one command-line string.
    //
    // We construct it from individual arguments, rather than invoking
    // a shell.
    std::string command_line;
    command_line += QuoteWindowsArgument(executable);

    for (const auto& arg : args)
    {
        command_line += ' ';
        command_line += QuoteWindowsArgument(arg);
    }

    STARTUPINFOA startup{};
    startup.cb = sizeof(startup);

    startup.dwFlags |= STARTF_USESTDHANDLES;
    startup.hStdOutput = stdout_write;
    startup.hStdError = stderr_write;
    startup.hStdInput = GetStdHandle(STD_INPUT_HANDLE);

    PROCESS_INFORMATION pi{};

    // CreateProcess may modify the command-line buffer.
    std::vector<char> mutable_command_line(command_line.begin(),
                                           command_line.end());

    mutable_command_line.push_back('\0');

    BOOL result = CreateProcessA(nullptr,
                                 mutable_command_line.data(),
                                 nullptr,
                                 nullptr,
                                 TRUE,
                                 0,
                                 nullptr,
                                 nullptr,
                                 &startup,
                                 &pi);

    // The child has its own handles now.
    CloseHandle(stdout_write);
    CloseHandle(stderr_write);

    if (!result)
    {
        CloseHandle(stdout_read);
        CloseHandle(stderr_read);

        state_ = State::Failed;
        return false;
    }

    CloseHandle(pi.hThread);

    process_handle_ = pi.hProcess;
    process_id_ = pi.dwProcessId;

    stdout_read_ = stdout_read;
    stderr_read_ = stderr_read;

    state_ = State::Running;
    exit_code_ = -1;

    stdout_thread_ = std::thread(&Process::StdoutThread, this);

    stderr_thread_ = std::thread(&Process::StderrThread, this);

    return true;
}

void Process::StdoutThread()
{
    HANDLE handle = static_cast<HANDLE>(stdout_read_);

    char buffer[4096];

    DWORD bytes_read = 0;

    while (ReadFile(handle, buffer, sizeof(buffer), &bytes_read, nullptr))
    {
        if (bytes_read == 0)
            break;

        if (stdout_stream_)
        {
            std::lock_guard lock(mutex_);

            stdout_stream_->write(buffer,
                                  static_cast<std::streamsize>(bytes_read));

            stdout_stream_->flush();
        }
    }

    CloseHandle(handle);
    stdout_read_ = nullptr;
}

void Process::StderrThread()
{
    HANDLE handle = static_cast<HANDLE>(stderr_read_);

    char buffer[4096];

    DWORD bytes_read = 0;

    while (ReadFile(handle, buffer, sizeof(buffer), &bytes_read, nullptr))
    {
        if (bytes_read == 0)
            break;

        if (stderr_stream_)
        {
            std::lock_guard lock(mutex_);

            stderr_stream_->write(buffer,
                                  static_cast<std::streamsize>(bytes_read));

            stderr_stream_->flush();
        }
    }

    CloseHandle(handle);
    stderr_read_ = nullptr;
}

void Process::UpdateState()
{
    std::lock_guard lock(mutex_);

    if (state_ != State::Running || !process_handle_)
        return;

    HANDLE handle = static_cast<HANDLE>(process_handle_);

    DWORD result = WaitForSingleObject(handle, 0);

    if (result != WAIT_OBJECT_0)
        return;

    DWORD exit_code = 0;

    if (GetExitCodeProcess(handle, &exit_code))
    {
        exit_code_ = static_cast<int>(exit_code);
        state_ = State::Exited;
    }
    else
    {
        state_ = State::Failed;
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

    if (state_ != State::Running || !process_handle_)
        return false;

    return TerminateProcess(static_cast<HANDLE>(process_handle_), 1) != FALSE;
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

    HANDLE handle = static_cast<HANDLE>(process_handle_);

    WaitForSingleObject(handle, INFINITE);

    DWORD exit_code = 0;

    if (!GetExitCodeProcess(handle, &exit_code))
    {
        state_ = State::Failed;
        return -1;
    }

    exit_code_ = static_cast<int>(exit_code);
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
    return process_id_;
}

int Process::ExitCode() const
{
    return exit_code_.load();
}

}  // namespace osh

#endif