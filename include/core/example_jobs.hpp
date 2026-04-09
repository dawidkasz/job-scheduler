#pragma once

#include <chrono>
#include <thread>

#include "core/job.hpp"
#include "core/job_result.hpp"

/// echoes args["msg"] if set, else empty ok()
class EchoJob : public Job {
public:
    EchoJob() : Job("echo") {}
    JobResult execute(const JobContext& ctx) override {
        if (ctx.args.contains("msg"))
            return JobResult::ok(ctx.args["msg"].get<std::string>());
        return JobResult::ok();
    }
};

/** sleeps args["seconds"] (default 1) */
class SleepJob : public Job {
public:
    SleepJob() : Job("sleep") {}
    JobResult execute(const JobContext& ctx) override {
        int sec = ctx.args.value("seconds", 1);
        std::this_thread::sleep_for(std::chrono::seconds(sec));
        return JobResult::ok();
    }
};

/// always fail — handy for retry tests
class FailJob : public Job {
public:
    FailJob() : Job("fail") {}
    JobResult execute(const JobContext&) override {
        return JobResult::fail("intentional failure");
    }
};
