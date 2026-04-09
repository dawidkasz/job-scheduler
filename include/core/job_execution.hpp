#pragma once

#include <chrono>
#include <memory>
#include <optional>

#include <nlohmann/json.hpp>

#include "core/job.hpp"
#include "core/job_id.hpp"
#include "core/job_result.hpp"
#include "core/job_status.hpp"

/** One scheduled run: stable JobId, args json, priority, status lifecycle. Scheduler is the only mutator. */
class JobExecution {
public:
    explicit JobExecution(
        std::shared_ptr<Job> job, int priority = 0,
        std::chrono::system_clock::time_point runAt = std::chrono::system_clock::now(),
        nlohmann::json args = {});

    const JobId& getId() const { return id_; }
    const std::string& getJobName() const { return job_->getName(); }
    int getPriority() const { return priority_; }
    std::chrono::system_clock::time_point getRunAt() const { return runAt_; }
    JobStatus getStatus() const { return status_; }
    int getRetryCount() const { return retryCount_; }
    const std::optional<JobResult>& getResult() const { return result_; }
    std::chrono::system_clock::time_point getCreatedAt() const { return createdAt_; }
    const nlohmann::json& getArgs() const { return args_; }

private:
    friend class Scheduler;

    Job& getJob() { return *job_; }
    void setStatus(JobStatus s) { status_ = s; }
    void setResult(JobResult r) { result_ = std::move(r); }
    void incrementRetry() { ++retryCount_; }

    JobId id_{JobId::generate()};
    std::shared_ptr<Job> job_;
    int priority_;
    std::chrono::system_clock::time_point runAt_;
    JobStatus status_{JobStatus::Pending};
    int retryCount_{0};
    std::optional<JobResult> result_;
    std::chrono::system_clock::time_point createdAt_{std::chrono::system_clock::now()};
    nlohmann::json args_;
};
