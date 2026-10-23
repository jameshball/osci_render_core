#pragma once

#include <juce_core/juce_core.h>
#include <vector>

#if JUCE_MAC || JUCE_LINUX
#include <cerrno>
#include <fcntl.h>
#include <signal.h>
#include <spawn.h>
#include <sys/wait.h>
#include <unistd.h>
#if JUCE_MAC
#include <crt_externs.h>
#else
extern char** environ;
#endif
#endif

namespace osci {

// A child program run to completion, its output thrown away or read back
// (standard output and error together). JUCE's POSIX ChildProcess reports a
// child killed by a signal (a crash, or out of memory) as having exited with
// 0, so on POSIX this keeps its own wait status: a killed child never passes
// off a partial file as done. Elsewhere it wraps juce::ChildProcess.
// One owner thread, except that one other thread may read() while the owner
// waits.
class Subprocess {
public:
    enum class Output { discard, capture };

    Subprocess() = default;
    ~Subprocess() {
        kill();
        wait(-1);
#if JUCE_MAC || JUCE_LINUX
        if (readEnd >= 0) {
            ::close(readEnd);
        }
#endif
    }

    bool start(const juce::StringArray& arguments, Output output) {
#if JUCE_MAC || JUCE_LINUX
        if (arguments.isEmpty() || pid > 0) {
            return false;
        }
        const bool capture = output == Output::capture;
        int ends[2] = {-1, -1};
        if (capture && !openPipe(ends)) {
            return false;
        }
        std::vector<char*> argv;
        for (const auto& argument : arguments) {
            argv.push_back(const_cast<char*>(argument.toRawUTF8()));
        }
        argv.push_back(nullptr);
        posix_spawn_file_actions_t actions;
        if (posix_spawn_file_actions_init(&actions) != 0) {
            closePipe(ends);
            return false;
        }
        const auto input = posix_spawn_file_actions_addopen(&actions, STDIN_FILENO, "/dev/null", O_RDONLY, 0);
        const auto standard = capture ? posix_spawn_file_actions_adddup2(&actions, ends[1], STDOUT_FILENO)
                                      : posix_spawn_file_actions_addopen(&actions, STDOUT_FILENO, "/dev/null", O_WRONLY, 0);
        const auto error = capture ? posix_spawn_file_actions_adddup2(&actions, ends[1], STDERR_FILENO)
                                   : posix_spawn_file_actions_addopen(&actions, STDERR_FILENO, "/dev/null", O_WRONLY, 0);
        posix_spawnattr_t attributes;
        if (posix_spawnattr_init(&attributes) != 0) {
            posix_spawn_file_actions_destroy(&actions);
            closePipe(ends);
            return false;
        }
#if JUCE_MAC
        // The child keeps only its standard streams, not the app's other
        // descriptors (such as live-source listening sockets).
        const auto flags = posix_spawnattr_setflags(&attributes, POSIX_SPAWN_CLOEXEC_DEFAULT);
        auto environment = *_NSGetEnviron();
#else
        const auto flags = 0;
        auto environment = environ;
#endif
        const auto spawned = input == 0 && standard == 0 && error == 0 && flags == 0
            ? posix_spawn(&pid, argv.front(), &actions, &attributes, argv.data(), environment) : EINVAL;
        posix_spawnattr_destroy(&attributes);
        posix_spawn_file_actions_destroy(&actions);
        if (ends[1] >= 0) {
            ::close(ends[1]);
            ends[1] = -1;
        }
        if (spawned != 0) {
            closePipe(ends);
            pid = -1;
            return false;
        }
        readEnd = ends[0];
        return true;
#else
        return process.start(arguments, output == Output::capture ? juce::ChildProcess::wantStdOut | juce::ChildProcess::wantStdErr : 0);
#endif
    }

    // Captured output only: 0 at its end.
    int read(void* buffer, int size) {
#if JUCE_MAC || JUCE_LINUX
        if (readEnd < 0) {
            return 0;
        }
        for (;;) {
            const auto count = ::read(readEnd, buffer, static_cast<std::size_t>(size));
            if (count < 0 && errno == EINTR) {
                continue;
            }
            return count > 0 ? static_cast<int>(count) : 0;
        }
#else
        return process.readProcessOutput(buffer, size);
#endif
    }

    // Whether it has finished (or never started), waiting up to
    // `milliseconds` for it; -1 waits for as long as it takes.
    bool wait(int milliseconds) {
#if JUCE_MAC || JUCE_LINUX
        // Never started: waitpid(-1) would reap some other child instead.
        if (pid <= 0 || finished) {
            return true;
        }
        const auto deadline = juce::Time::getMillisecondCounterHiRes() + milliseconds;
        do {
            int status = 0;
            const auto result = waitpid(pid, &status, WNOHANG);
            if (result == pid) {
                succeeded = WIFEXITED(status) && WEXITSTATUS(status) == 0;
                finished = true;
                return true;
            }
            if (result < 0 && errno != EINTR) {
                finished = true;
                return true;
            }
            juce::Thread::sleep(2);
        } while (milliseconds < 0 || juce::Time::getMillisecondCounterHiRes() < deadline);
        return false;
#else
        const auto complete = process.waitForProcessToFinish(milliseconds);
        if (complete) {
            succeeded = process.getExitCode() == 0;
        }
        return complete;
#endif
    }

    // Finished by exiting with 0, not killed or crashed.
    bool exitedCleanly() const { return succeeded; }

    void kill() {
#if JUCE_MAC || JUCE_LINUX
        if (pid > 0 && !finished) {
            ::kill(pid, SIGKILL);
        }
#else
        if (process.isRunning()) {
            process.kill();
        }
#endif
    }

private:
#if JUCE_MAC || JUCE_LINUX
    // Read by this end, written by the child's standard output and error;
    // neither end leaks into other children.
    static bool openPipe(int ends[2]) {
#if JUCE_LINUX
        return pipe2(ends, O_CLOEXEC) == 0;
#else
        if (pipe(ends) != 0) {
            return false;
        }
        fcntl(ends[0], F_SETFD, FD_CLOEXEC);
        fcntl(ends[1], F_SETFD, FD_CLOEXEC);
        return true;
#endif
    }
    static void closePipe(int ends[2]) {
        for (int index = 0; index < 2; ++index) {
            if (ends[index] >= 0) {
                ::close(ends[index]);
                ends[index] = -1;
            }
        }
    }

    pid_t pid = -1;
    int readEnd = -1;
    bool finished = false;
#else
    juce::ChildProcess process;
#endif
    bool succeeded = false;

    JUCE_DECLARE_NON_COPYABLE(Subprocess)
};

}
