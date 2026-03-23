#pragma once

#include <nlohmann/json.hpp>
#include <string>
#include <unordered_map>

#include "core/job_result.hpp"

struct JobContext {
    nlohmann::json args;
    std::unordered_map<std::string, JobResult> dependencyResults;
};
