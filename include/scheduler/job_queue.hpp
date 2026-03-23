#pragma once

#include <condition_variable>
#include <memory>
#include <mutex>
#include <vector>

#include "core/job_execution.hpp"
#include "core/job_id.hpp"

class JobQueue {
public:
    void push(std::shared_ptr<JobExecution> exec);
    std::shared_ptr<JobExecution> pop();
    bool remove(const JobId& id);
    std::size_t size() const;
    bool empty() const;

private:
    std::vector<std::shared_ptr<JobExecution>> heap_;
    mutable std::mutex mutex_;
    std::condition_variable cv_;
};
