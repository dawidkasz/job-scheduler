#pragma once

#include <chrono>
#include <optional>
#include <string>

#include "core/job_id.hpp"
#include "core/job_result.hpp"
#include "core/job_status.hpp"

struct JobMetadata {
    JobId id;
    std::string jobName;
    int priority{0};
    JobStatus status{JobStatus::Pending};
    int retryCount{0};
    std::chrono::system_clock::time_point createdAt;
    std::optional<JobResult> result;
};
