#pragma once

#include <nlohmann/json.hpp>
#include <string>
#include <unordered_map>

#include "core/job_result.hpp"

/// Args for this run plus completed dependency results keyed by job name (rough cut).
struct JobContext {
    nlohmann::json args;
    std::unordered_map<std::string, JobResult> dependencyResults;
};
