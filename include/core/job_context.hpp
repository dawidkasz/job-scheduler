#pragma once

#include <string>
#include <unordered_map>

#include "core/job_result.hpp"

struct JobContext {
    std::unordered_map<std::string, JobResult> dependencyResults;
};
