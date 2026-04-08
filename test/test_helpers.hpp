#pragma once

#include "core/job.hpp"
#include "core/job_result.hpp"

class DummyJob : public Job {
public:
    explicit DummyJob(std::string name = "dummy") : Job(std::move(name)) {}
    JobResult execute(const JobContext&) override { return JobResult::ok(); }
};

class FailingJob : public Job {
public:
    explicit FailingJob(std::string name = "failing") : Job(std::move(name)) {}
    JobResult execute(const JobContext&) override { return JobResult::fail("intentional failure"); }
};

class ValueJob : public Job {
public:
    explicit ValueJob(std::string name = "value") : Job(std::move(name)) {}
    JobResult execute(const JobContext&) override { return JobResult::ok(std::string("some job result")); }
};
