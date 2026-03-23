#pragma once

#include <sys/types.h>

#include <cstddef>
#include <functional>
#include <mutex>
#include <unordered_set>

#include "core/job_result.hpp"

class ProcessPool {
public:
    explicit ProcessPool(std::size_t maxProcesses);
    ~ProcessPool();

    ProcessPool(const ProcessPool&) = delete;
    ProcessPool& operator=(const ProcessPool&) = delete;

    struct ChildProcessHandle {
        pid_t pid{-1};
        int resultFd{-1};
    };

    ChildProcessHandle spawn(std::function<JobResult()> work);
    void terminate(const ChildProcessHandle& handle);
    JobResult readResult(const ChildProcessHandle& handle);
    std::size_t runningCount() const;
    std::size_t maxProcesses() const { return maxProcesses_; }
    void shutdownAll();

private:
    std::size_t maxProcesses_;
    mutable std::mutex mutex_;
    std::unordered_set<pid_t> activeChildren_;
};
