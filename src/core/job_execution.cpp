#include "core/job_execution.hpp"

JobExecution::JobExecution(std::shared_ptr<Job> job, int priority,
                           std::chrono::system_clock::time_point runAt)
    : job_(std::move(job)), priority_(priority), runAt_(runAt) {}
