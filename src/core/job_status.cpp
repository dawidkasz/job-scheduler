#include "core/job_status.hpp"

#include <stdexcept>

const char* jobStatusToString(JobStatus status) {
    switch (status) {
        case JobStatus::Pending:   return "Pending";
        case JobStatus::Running:   return "Running";
        case JobStatus::Completed: return "Completed";
        case JobStatus::Failed:    return "Failed";
        case JobStatus::Cancelled: return "Cancelled";
    }
    throw std::invalid_argument("Unknown JobStatus");
}
