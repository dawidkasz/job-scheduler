#pragma once

#include <chrono>
#include <thread>

#include "core/job.hpp"
#include "core/job_result.hpp"

/// @brief Returns the "msg" argument as the result payload.
class EchoJob : public Job {
public:
    EchoJob() : Job("echo") {}
    JobResult execute(const JobContext& ctx) override {
        if (ctx.args.contains("msg"))
            return JobResult::ok(ctx.args["msg"].get<std::string>());
        return JobResult::ok();
    }
};

/// @brief Sleeps for the number of seconds given in the "seconds" argument.
class SleepJob : public Job {
public:
    SleepJob() : Job("sleep") {}
    JobResult execute(const JobContext& ctx) override {
        int sec = ctx.args.value("seconds", 1);
        std::this_thread::sleep_for(std::chrono::seconds(sec));
        return JobResult::ok();
    }
};

/// @brief Always fails; useful for testing retry logic.
class FailJob : public Job {
public:
    FailJob() : Job("fail") {}
    JobResult execute(const JobContext&) override {
        return JobResult::fail("intentional failure");
    }
};
