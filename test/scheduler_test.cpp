#include <gtest/gtest.h>

#include <chrono>
#include <thread>
#include <variant>

#include "core/job_metadata.hpp"
#include "scheduler/process_pool.hpp"
#include "scheduler/scheduler.hpp"
#include "storage/in_memory_execution_repository.hpp"
#include "storage/job_registry.hpp"
#include "test_helpers.hpp"

class JobA : public Job {
public:
    JobA() : Job("a") {}
    JobResult execute(const JobContext& ctx) override {
        return JobResult::ok(ctx.args["value"].get<int>());
    }
};

class JobB : public Job {
public:
    JobB() : Job("b") {}
    JobResult execute(const JobContext& ctx) override {
        return JobResult::ok(ctx.args["value"].get<int>());
    }
};

class JobC : public Job {
public:
    JobC() : Job("c") {}
    JobResult execute(const JobContext& ctx) override {
        int sum = 0;
        for (const auto& [name, res] : ctx.dependencyResults) {
            if (std::holds_alternative<int>(res.value)) {
                sum += std::get<int>(res.value);
            }
        }
        return JobResult::ok(sum);
    }
};

TEST(ProcessPool, SpawnAndReadSuccess) {
    ProcessPool pool(4);
    auto handle = pool.spawn([] { return JobResult::ok(1); });
    auto result = pool.readResult(handle);
    EXPECT_TRUE(result.success);
}

TEST(ProcessPool, SpawnAndReadFailure) {
    ProcessPool pool(4);
    auto handle = pool.spawn([] { return JobResult::fail("error"); });
    auto result = pool.readResult(handle);
    EXPECT_FALSE(result.success);
}

TEST(Scheduler, SubmitAndComplete) {
    ProcessPool pool(2);
    JobRegistry reg;
    reg.registerType("dummy", [] { return std::make_shared<DummyJob>(); });
    InMemoryExecutionRepository repo;
    Scheduler sched(pool, reg, repo, 3);
    sched.start();
    auto jobId = sched.startJobByName("dummy");
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    EXPECT_EQ(sched.getJobById(jobId).value().status, JobStatus::Completed);
    sched.stop();
}

TEST(Scheduler, ResultStoredInRepository) {
    ProcessPool pool(2);
    JobRegistry reg;
    reg.registerType("value", [] { return std::make_shared<ValueJob>(); });
    InMemoryExecutionRepository repo;
    Scheduler sched(pool, reg, repo, 1);
    sched.start();
    auto jobId = sched.startJobByName("value");
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    auto meta = sched.getJobById(jobId);
    ASSERT_TRUE(meta.has_value());
    ASSERT_TRUE(meta->result.has_value());
    EXPECT_TRUE(meta->result->success);
    EXPECT_EQ(std::get<std::string>(meta->result->value), "some job result");
    sched.stop();
}

TEST(Scheduler, JobWithTwoDependencies) {
    ProcessPool pool(4);
    JobRegistry reg;
    reg.registerType("a", [] { return std::make_shared<JobA>(); });
    reg.registerType("b", [] { return std::make_shared<JobB>(); });
    reg.registerType("c", [] { return std::make_shared<JobC>(); });
    InMemoryExecutionRepository repo;
    Scheduler sched(pool, reg, repo, 1);
    sched.start();
    auto idA = sched.startJobByName("a", {{"value", 10}});
    auto idB = sched.startJobByName("b", {{"value", 17}});
    auto idC = sched.startJobByName("c", {idA, idB});
    std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    auto meta = sched.getJobById(idC);
    ASSERT_TRUE(meta.has_value());
    EXPECT_EQ(meta->status, JobStatus::Completed);
    ASSERT_TRUE(meta->result.has_value());
    EXPECT_EQ(std::get<int>(meta->result->value), 27);
    sched.stop();
}

TEST(Scheduler, RetryOnFailure) {
    ProcessPool pool(2);
    JobRegistry reg;
    reg.registerType("failing", [] { return std::make_shared<FailingJob>(); });
    InMemoryExecutionRepository repo;
    Scheduler sched(pool, reg, repo, 4);
    sched.start();
    auto jobId = sched.startJobByName("failing");
    std::this_thread::sleep_for(std::chrono::milliseconds(2000));
    auto meta = sched.getJobById(jobId);
    ASSERT_TRUE(meta.has_value());
    EXPECT_EQ(meta->status, JobStatus::Failed);
    EXPECT_EQ(meta->retryCount, 4);
    sched.stop();
}
