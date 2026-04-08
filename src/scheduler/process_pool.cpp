#include "scheduler/process_pool.hpp"

#include <sys/wait.h>
#include <unistd.h>

#include <csignal>
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
}

ProcessPool::ProcessPool(std::size_t maxProcesses) : maxProcesses_(maxProcesses) {}

ProcessPool::~ProcessPool() {
    shutdownAll();
}

ProcessPool::ChildProcessHandle ProcessPool::spawn(std::function<JobResult()> work) {
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
        write(pipefd[1], data.c_str(), data.size());
        close(pipefd[1]);
        _exit(0);
    }

    close(pipefd[1]);
    std::lock_guard lock(mutex_);
    activeChildren_.insert(pid);
    return {pid, pipefd[0]};
}

JobResult ProcessPool::readResult(const ChildProcessHandle& handle) {
    std::string data;
    char buf[256];
    ssize_t n;
    while ((n = read(handle.resultFd, buf, sizeof(buf))) > 0) {
        data.append(buf, static_cast<std::size_t>(n));
    }
    close(handle.resultFd);
    waitpid(handle.pid, nullptr, 0);
    std::lock_guard lock(mutex_);
    activeChildren_.erase(handle.pid);
    try {
        return deserializeResult(nlohmann::json::parse(data));
    } catch (...) {
        return JobResult::fail("child process produced invalid result");
    }
}

void ProcessPool::terminate(const ChildProcessHandle& handle) {
    kill(handle.pid, SIGTERM);
    close(handle.resultFd);
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
