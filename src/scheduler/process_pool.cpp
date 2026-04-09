#include "scheduler/process_pool.hpp"

#include <sys/wait.h>
#include <unistd.h>

#include <cerrno>
#include <csignal>
#include <fcntl.h>
#include <poll.h>
#include <stdexcept>
#include <variant>

#include <nlohmann/json.hpp>

namespace {
nlohmann::json serializeResult(const JobResult& r) {
    nlohmann::json j;
    j["success"] = r.success;
    j["has_error"] = r.error.has_value();
    j["error"] = r.error.value_or("");
    std::visit([&j](auto&& v) {
        using T = std::decay_t<decltype(v)>;
        if constexpr (std::is_same_v<T, std::monostate>) {
            j["value"] = nullptr;
        } else {
            j["value"] = v;
        }
    }, r.value);
    return j;
}

JobResult deserializeResult(const nlohmann::json& j) {
    JobResult r;
    r.success = j["success"].get<bool>();
    if (j["has_error"].get<bool>()) {
        r.error = j["error"].get<std::string>();
    }
    const auto& val = j["value"];
    if (val.is_string()) {
        r.value = val.get<std::string>();
    } else if (val.is_number_integer()) {
        r.value = val.get<int>();
    } else if (val.is_number_float()) {
        r.value = val.get<double>();
    }
    return r;
}

void writeAll(int fd, const char* data, std::size_t len) {
    std::size_t off = 0;
    while (off < len) {
        ssize_t n = write(fd, data + off, len - off);
        if (n < 0) {
            if (errno == EINTR) continue;
            _exit(1);
        }
        off += static_cast<std::size_t>(n);
    }
}
}

ProcessPool::ProcessPool(std::size_t maxProcesses) : maxProcesses_(maxProcesses) {}

ProcessPool::~ProcessPool() {
    shutdownAll();
}

bool ProcessPool::trySpawn(std::function<JobResult()> work, ChildProcessHandle& out) {
    {
        std::lock_guard lock(mutex_);
        if (activeChildren_.size() >= maxProcesses_) return false;
    }

    int pipefd[2];
    if (pipe(pipefd) != 0)
        throw std::runtime_error("pipe failed");

    pid_t pid = fork();
    if (pid < 0)
        throw std::runtime_error("fork failed");

    if (pid == 0) {
        close(pipefd[0]);
        JobResult result = work();
        std::string data = serializeResult(result).dump();
        writeAll(pipefd[1], data.data(), data.size());
        close(pipefd[1]);
        _exit(0);
    }

    close(pipefd[1]);
    if (fcntl(pipefd[0], F_SETFL, O_NONBLOCK) != 0) {
        close(pipefd[0]);
        kill(pid, SIGKILL);
        waitpid(pid, nullptr, 0);
        throw std::runtime_error("fcntl O_NONBLOCK failed");
    }

    std::lock_guard lock(mutex_);
    activeChildren_.insert(pid);
    out = {pid, pipefd[0]};
    return true;
}

ProcessPool::ChildProcessHandle ProcessPool::spawn(std::function<JobResult()> work) {
    ChildProcessHandle h;
    if (!trySpawn(std::move(work), h))
        throw std::runtime_error("process pool at capacity");
    return h;
}

bool ProcessPool::tryDrain(ChildProcessHandle& handle, std::string& buffer, JobResult& resultOut) {
    if (handle.resultFd < 0) return true;

    for (;;) {
        char chunk[256];
        ssize_t n = read(handle.resultFd, chunk, sizeof(chunk));
        if (n > 0) {
            buffer.append(chunk, static_cast<std::size_t>(n));
            continue;
        }
        if (n == 0) {
            close(handle.resultFd);
            handle.resultFd = -1;
            waitpid(handle.pid, nullptr, 0);
            pid_t reapPid = handle.pid;
            handle.pid = -1;
            {
                std::lock_guard lock(mutex_);
                activeChildren_.erase(reapPid);
            }
            try {
                resultOut = deserializeResult(nlohmann::json::parse(buffer));
            } catch (...) {
                resultOut = JobResult::fail("child process produced invalid result");
            }
            return true;
        }
        if (errno == EINTR) continue;
        if (errno == EAGAIN) return false;

        close(handle.resultFd);
        handle.resultFd = -1;
        kill(handle.pid, SIGKILL);
        waitpid(handle.pid, nullptr, 0);
        {
            std::lock_guard lock(mutex_);
            activeChildren_.erase(handle.pid);
        }
        handle.pid = -1;
        resultOut = JobResult::fail("read from child pipe failed");
        return true;
    }
}

JobResult ProcessPool::readResult(const ChildProcessHandle& handle) {
    ChildProcessHandle h = handle;
    std::string buf;
    JobResult result;
    struct pollfd pfd {};
    pfd.fd = h.resultFd;
    pfd.events = POLLIN | POLLHUP;

    while (!tryDrain(h, buf, result)) {
        int pr = 0;
        do {
            pr = poll(&pfd, 1, -1);
        } while (pr < 0 && errno == EINTR);
        if (pr < 0) {
            if (h.resultFd >= 0) close(h.resultFd);
            if (h.pid >= 0) {
                kill(h.pid, SIGKILL);
                waitpid(h.pid, nullptr, 0);
                std::lock_guard lock(mutex_);
                activeChildren_.erase(h.pid);
            }
            return JobResult::fail("poll on child pipe failed");
        }
        pfd.revents = 0;
    }
    return result;
}

void ProcessPool::terminate(const ChildProcessHandle& handle) {
    if (handle.pid < 0) return;
    kill(handle.pid, SIGTERM);
    if (handle.resultFd >= 0) close(handle.resultFd);
    waitpid(handle.pid, nullptr, 0);
    std::lock_guard lock(mutex_);
    activeChildren_.erase(handle.pid);
}

std::size_t ProcessPool::runningCount() const {
    std::lock_guard lock(mutex_);
    return activeChildren_.size();
}

void ProcessPool::shutdownAll() {
    std::lock_guard lock(mutex_);
    for (pid_t pid : activeChildren_) {
        kill(pid, SIGTERM);
        waitpid(pid, nullptr, 0);
    }
    activeChildren_.clear();
}
