#pragma once

#include <string>

#include "core/job_context.hpp"
#include "core/job_result.hpp"

class Job {
public:
    explicit Job(std::string name) : name_(std::move(name)) {}
    virtual ~Job() = default;

    Job(const Job&) = delete;
    Job& operator=(const Job&) = delete;

    virtual JobResult execute(const JobContext& ctx) = 0;

    const std::string& getName() const { return name_; }

private:
    std::string name_;
};
