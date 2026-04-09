#pragma once

#include <sys/types.h>

#include <cstddef>
#include <functional>
#include <mutex>
#include <string>
#include <unordered_set>

#include "core/job_result.hpp"

/// Fork pool with a max concurrent children; work runs in the child, result comes back over a pipe.
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

    /// Returns false if the pool is already at maxProcesses (does not fork).
    bool trySpawn(std::function<JobResult()> work, ChildProcessHandle& out);

    /// Blocking spawn; throws std::runtime_error if the pool is full.
    ChildProcessHandle spawn(std::function<JobResult()> work);

    /// Non-blocking read: append to buffer; on EOF reap child and set resultOut. Returns true if finished.
    bool tryDrain(ChildProcessHandle& handle, std::string& buffer, JobResult& resultOut);

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
