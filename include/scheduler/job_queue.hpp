#pragma once

#include <chrono>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <optional>
#include <vector>

#include "core/job_execution.hpp"
#include "core/job_id.hpp"

/// Priority + scheduled-time heap; blocking pop with optional timeout.
class JobQueue {
public:
    void push(std::shared_ptr<JobExecution> exec);
    std::shared_ptr<JobExecution> pop();
    std::optional<std::shared_ptr<JobExecution>> tryPop();
    std::optional<std::shared_ptr<JobExecution>> popFor(std::chrono::milliseconds timeout);
    bool remove(const JobId& id);
    std::size_t size() const;
    bool empty() const;

private:
    std::vector<std::shared_ptr<JobExecution>> heap_;
    mutable std::mutex mutex_;
    std::condition_variable cv_;
};
