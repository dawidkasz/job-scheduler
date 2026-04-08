#include "core/job_execution.hpp"

#include <nlohmann/json.hpp>

JobExecution::JobExecution(std::shared_ptr<Job> job, int priority,
                           std::chrono::system_clock::time_point runAt,
                           nlohmann::json args)
    : job_(std::move(job)), priority_(priority), runAt_(runAt), args_(std::move(args)) {}
