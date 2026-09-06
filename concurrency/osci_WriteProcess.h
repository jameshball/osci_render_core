#pragma once

#include <juce_core/juce_core.h>
#include <algorithm>
#include <cerrno>
#include <climits>

#if JUCE_WINDOWS
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <fcntl.h>
#include <poll.h>
#include <pthread.h>
#include <signal.h>
#include <spawn.h>
#include <sys/wait.h>
#include <unistd.h>
#if JUCE_MAC
#include <crt_externs.h>
#endif
#endif

namespace osci {

// Single-owner pipe writer. No work or borrowed frame memory survives write().
class WriteProcess {
public:
    WriteProcess() = default;
    ~WriteProcess() { close(); }

    bool start(juce::String command) {
        close();
#if JUCE_WINDOWS
        const auto name = "\\\\.\\pipe\\osci-write-" + juce::Uuid().toString();
        pipe = CreateNamedPipeA(name.toRawUTF8(), PIPE_ACCESS_OUTBOUND | FILE_FLAG_OVERLAPPED,
                                PIPE_TYPE_BYTE | PIPE_WAIT, 1, 65536, 65536, 0, nullptr);
        event = CreateEvent(nullptr, TRUE, FALSE, nullptr);
        job = CreateJobObject(nullptr, nullptr);
        if (pipe == INVALID_HANDLE_VALUE || event == nullptr || job == nullptr) {
            close(0);
            return false;
        }
        JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits {};
        limits.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
        if (!SetInformationJobObject(job, JobObjectExtendedLimitInformation, &limits, sizeof(limits))) {
            close(0);
            return false;
        }

        SECURITY_ATTRIBUTES attributes { sizeof(SECURITY_ATTRIBUTES), nullptr, TRUE };
        const auto input = CreateFileA(name.toRawUTF8(), GENERIC_READ, 0, &attributes, OPEN_EXISTING, 0, nullptr);
        if (input == INVALID_HANDLE_VALUE) {
            close(0);
            return false;
        }
        OVERLAPPED connection {};
        connection.hEvent = event;
        const bool connected = ConnectNamedPipe(pipe, &connection) || GetLastError() == ERROR_PIPE_CONNECTED;
        if (!connected) {
            CancelIoEx(pipe, &connection);
            DWORD ignored = 0;
            GetOverlappedResult(pipe, &connection, &ignored, TRUE);
            CloseHandle(input);
            close(0);
            return false;
        }
        STARTUPINFOW startup {};
        startup.cb = sizeof(startup);
        startup.dwFlags = STARTF_USESTDHANDLES;
        startup.hStdInput = input;
        startup.hStdOutput = GetStdHandle(STD_OUTPUT_HANDLE);
        startup.hStdError = GetStdHandle(STD_ERROR_HANDLE);
        const bool missingOutput = startup.hStdOutput == nullptr || startup.hStdOutput == INVALID_HANDLE_VALUE;
        const bool missingError = startup.hStdError == nullptr || startup.hStdError == INVALID_HANDLE_VALUE;
        HANDLE nullOutput = INVALID_HANDLE_VALUE;
        if (missingOutput || missingError) {
            // GUI hosts may have no console. Supply valid inheritable output handles.
            nullOutput = CreateFileW(L"NUL", GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE,
                                     &attributes, OPEN_EXISTING, 0, nullptr);
            if (nullOutput == INVALID_HANDLE_VALUE) {
                CloseHandle(input);
                close(0);
                return false;
            }
            if (missingOutput) {
                startup.hStdOutput = nullOutput;
            }
            if (missingError) {
                startup.hStdError = nullOutput;
            }
        }
        PROCESS_INFORMATION child {};
        std::wstring cmd(("cmd /c \"" + command + "\"").toWideCharPointer());
        const bool started = CreateProcessW(nullptr, cmd.data(), nullptr, nullptr, TRUE,
            CREATE_NO_WINDOW | CREATE_SUSPENDED, nullptr, nullptr, &startup, &child);
        // Only the child may keep the read end, otherwise broken pipes never surface.
        CloseHandle(input);
        if (nullOutput != INVALID_HANDLE_VALUE) {
            CloseHandle(nullOutput);
        }
        if (!started) {
            close(0);
            return false;
        }
        process = child.hProcess;
        const bool assigned = AssignProcessToJobObject(job, process) != FALSE;
        const bool resumed = assigned && ResumeThread(child.hThread) != static_cast<DWORD>(-1);
        CloseHandle(child.hThread);
        if (!resumed) {
            TerminateProcess(process, 1);
            close(0);
            return false;
        }
#else
        int descriptors[2];
        if (::pipe(descriptors) != 0) {
            return false;
        }
        if (fcntl(descriptors[0], F_SETFD, FD_CLOEXEC) != 0
            || fcntl(descriptors[1], F_SETFD, FD_CLOEXEC) != 0
            || fcntl(descriptors[1], F_SETFL, O_NONBLOCK) != 0) {
            ::close(descriptors[0]);
            ::close(descriptors[1]);
            return false;
        }
        posix_spawn_file_actions_t actions;
        posix_spawnattr_t attributes;
        const bool actionsReady = posix_spawn_file_actions_init(&actions) == 0;
        const bool attributesReady = posix_spawnattr_init(&attributes) == 0;
        const bool configured = actionsReady && attributesReady
            && posix_spawn_file_actions_adddup2(&actions, descriptors[0], STDIN_FILENO) == 0
            && (descriptors[0] == STDIN_FILENO || posix_spawn_file_actions_addclose(&actions, descriptors[0]) == 0)
            && posix_spawn_file_actions_addclose(&actions, descriptors[1]) == 0
            && posix_spawnattr_setflags(&attributes, POSIX_SPAWN_SETPGROUP) == 0
            && posix_spawnattr_setpgroup(&attributes, 0) == 0;
        if (!configured) {
            if (actionsReady) {
                posix_spawn_file_actions_destroy(&actions);
            }
            if (attributesReady) {
                posix_spawnattr_destroy(&attributes);
            }
            ::close(descriptors[0]);
            ::close(descriptors[1]);
            return false;
        }
        auto cmd = command.toStdString();
        char shell[] = "/bin/sh";
        char option[] = "-c";
        char* arguments[] { shell, option, cmd.data(), nullptr };
#if JUCE_MAC
        auto environment = *_NSGetEnviron();
#else
        auto environment = environ;
#endif
        const auto error = posix_spawn(&process, shell, &actions, &attributes, arguments, environment);
        posix_spawnattr_destroy(&attributes);
        posix_spawn_file_actions_destroy(&actions);
        ::close(descriptors[0]);
        if (error != 0) {
            ::close(descriptors[1]);
            process = -1;
            return false;
        }
        pipe = descriptors[1];
#endif
        return true;
    }

    size_t write(const void* data, size_t size, int timeoutMs = 5000) {
        if (!isRunning()) {
            return 0;
        }
        const auto deadline = juce::Time::getMillisecondCounterHiRes() + std::max(0, timeoutMs);
        const auto* bytes = static_cast<const unsigned char*>(data);
        size_t written = 0;
#if !JUCE_WINDOWS
        ScopedPipeSignal signal;
#endif
        while (written < size) {
#if JUCE_WINDOWS
            OVERLAPPED operation {};
            operation.hEvent = event;
            ResetEvent(event);
            DWORD count = 0;
            const auto chunk = static_cast<DWORD>(std::min<size_t>(size - written, MAXDWORD));
            if (!WriteFile(pipe, bytes + written, chunk, &count, &operation)) {
                if (GetLastError() != ERROR_IO_PENDING) {
                    break;
                }
                if (WaitForSingleObject(event, remainingMs(deadline)) != WAIT_OBJECT_0) {
                    CancelIoEx(pipe, &operation);
                    // Cancellation is asynchronous: keep the OVERLAPPED and frame alive until acknowledged.
                    GetOverlappedResult(pipe, &operation, &count, TRUE);
                    break;
                }
                if (!GetOverlappedResult(pipe, &operation, &count, FALSE)) {
                    break;
                }
            }
            if (count == 0) {
                break;
            }
#else
            const auto count = ::write(pipe, bytes + written, std::min<size_t>(size - written, INT_MAX));
            if (count < 0) {
                if (errno == EINTR && remainingMs(deadline) > 0) {
                    continue;
                }
                if (errno == EAGAIN || errno == EWOULDBLOCK) {
                    pollfd descriptor { pipe, POLLOUT, 0 };
                    int ready;
                    do {
                        ready = poll(&descriptor, 1, remainingMs(deadline));
                    } while (ready < 0 && errno == EINTR && remainingMs(deadline) > 0);
                    if (ready > 0 && (descriptor.revents & POLLOUT) != 0 && remainingMs(deadline) > 0) {
                        continue;
                    }
                }
                signal.brokenPipe = errno == EPIPE;
                break;
            }
            if (count == 0) {
                break;
            }
#endif
            written += static_cast<size_t>(count);
            if (written < size && remainingMs(deadline) == 0) {
                break;
            }
        }
        if (written != size) {
            // A partial frame cannot be retried without corrupting the encoder stream.
            close(0);
            return 0;
        }
        return written;
    }

    bool close(int timeoutMs = 5000) {
        bool succeeded = true;
#if JUCE_WINDOWS
        if (pipe != INVALID_HANDLE_VALUE) {
            CloseHandle(pipe);
            pipe = INVALID_HANDLE_VALUE;
        }
        if (process != nullptr) {
            DWORD exitCode = 1;
            succeeded = WaitForSingleObject(process, static_cast<DWORD>(std::max(0, timeoutMs))) == WAIT_OBJECT_0
                && GetExitCodeProcess(process, &exitCode) && exitCode == 0;
            if (!succeeded) {
                TerminateJobObject(job, 1);
            }
            CloseHandle(process);
            process = nullptr;
        }
        if (job != nullptr) {
            CloseHandle(job);
            job = nullptr;
        }
        if (event != nullptr) {
            CloseHandle(event);
            event = nullptr;
        }
#else
        if (pipe >= 0) {
            ::close(pipe);
            pipe = -1;
        }
        if (process > 0) {
            const auto deadline = juce::Time::getMillisecondCounterHiRes() + std::max(0, timeoutMs);
            int status = 0;
            pid_t result;
            do {
                result = waitpid(process, &status, WNOHANG);
                if (result == process) {
                    succeeded = WIFEXITED(status) && WEXITSTATUS(status) == 0;
                    break;
                }
                if (result < 0 && errno != EINTR) {
                    succeeded = false;
                    break;
                }
                if (remainingMs(deadline) == 0) {
                    succeeded = false;
                    kill(-process, SIGKILL);
                    do {
                        result = waitpid(process, &status, 0);
                    } while (result < 0 && errno == EINTR);
                    break;
                }
                juce::Thread::sleep(std::min(10, remainingMs(deadline)));
            } while (true);
            process = -1;
        }
#endif
        return succeeded;
    }

    bool isRunning() const {
#if JUCE_WINDOWS
        return pipe != INVALID_HANDLE_VALUE && process != nullptr;
#else
        return pipe >= 0 && process > 0;
#endif
    }

private:
    static int remainingMs(double deadline) {
        return static_cast<int>(std::max(0.0, std::min(static_cast<double>(INT_MAX), deadline - juce::Time::getMillisecondCounterHiRes())));
    }

#if JUCE_WINDOWS
    HANDLE pipe = INVALID_HANDLE_VALUE;
    HANDLE process = nullptr;
    HANDLE event = nullptr;
    HANDLE job = nullptr;
#else
    // Block SIGPIPE only on this writing thread, without changing the host's signal handler.
    struct ScopedPipeSignal {
        ScopedPipeSignal() {
            sigemptyset(&blocked);
            sigaddset(&blocked, SIGPIPE);
            pthread_sigmask(SIG_BLOCK, &blocked, &previous);
            sigset_t pending;
            sigpending(&pending);
            wasPending = sigismember(&pending, SIGPIPE) != 0;
        }
        ~ScopedPipeSignal() {
            if (brokenPipe && !wasPending) {
                sigset_t pending;
                sigpending(&pending);
                if (sigismember(&pending, SIGPIPE) != 0) {
                    int received;
                    sigwait(&blocked, &received);
                }
            }
            pthread_sigmask(SIG_SETMASK, &previous, nullptr);
        }
        sigset_t blocked {}, previous {};
        bool wasPending = false;
        bool brokenPipe = false;
    };
    int pipe = -1;
    pid_t process = -1;
#endif

    JUCE_DECLARE_NON_COPYABLE(WriteProcess)
};

} // namespace osci
