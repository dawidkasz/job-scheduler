#pragma once

#include <string>
#include <vector>

#include "core/job_context.hpp"
#include "core/job_result.hpp"

class Job {
public:
    explicit Job(std::string name, std::vector<std::string> dependencies = {})
        : name_(std::move(name)), dependencies_(std::move(dependencies)) {}
    virtual ~Job() = default;

    Job(const Job&) = delete;
    Job& operator=(const Job&) = delete;

    virtual JobResult execute(const JobContext& ctx) = 0;

    const std::string& getName() const { return name_; }
    const std::vector<std::string>& getDependencies() const { return dependencies_; }

private:
    std::string name_;
    std::vector<std::string> dependencies_;
};
