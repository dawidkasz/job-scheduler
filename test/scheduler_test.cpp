#include <gtest/gtest.h>

#include <chrono>
#include <thread>
#include <unordered_map>
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

class StaticParentJob : public Job {
public:
    StaticParentJob() : Job("static_parent") {}
    JobResult execute(const JobContext& ctx) override {
        return JobResult::ok(ctx.args.value("base", 100));
    }
};

class StaticChildJobA : public Job {
public:
    StaticChildJobA() : Job("static_child_a") {}
    JobResult execute(const JobContext& ctx) override {
        const int base = std::get<int>(ctx.dependencyResults.at("static_parent").value);
        return JobResult::ok(base + 1);
    }
};

class StaticChildJobB : public Job {
public:
    StaticChildJobB() : Job("static_child_b") {}
    JobResult execute(const JobContext& ctx) override {
        const int base = std::get<int>(ctx.dependencyResults.at("static_parent").value);
        return JobResult::ok(base + 2);
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

TEST(Scheduler, ParentThenChildrenViaExplicitJobIds) {
    ProcessPool pool(4);
    JobRegistry reg;
    reg.registerType("static_parent", [] { return std::make_shared<StaticParentJob>(); });
    reg.registerType("static_child_a", [] { return std::make_shared<StaticChildJobA>(); });
    reg.registerType("static_child_b", [] { return std::make_shared<StaticChildJobB>(); });
    InMemoryExecutionRepository repo;
    Scheduler sched(pool, reg, repo, 1);
    sched.start();
    auto idParent = sched.startJobByName("static_parent", {{"base", 10}});
    sched.startJobByName("static_child_a", {idParent}, {});
    sched.startJobByName("static_child_b", {idParent}, {});
    std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    auto parentMeta = sched.getJobById(idParent);
    ASSERT_TRUE(parentMeta.has_value());
    EXPECT_EQ(parentMeta->status, JobStatus::Completed);
    ASSERT_TRUE(parentMeta->result.has_value());
    EXPECT_EQ(std::get<int>(parentMeta->result->value), 10);
    EXPECT_EQ(sched.listJobs().size(), 3u);

    std::unordered_map<std::string, int> byName;
    for (const auto& meta : sched.listJobs()) {
        ASSERT_TRUE(meta.result.has_value());
        byName[meta.jobName] = std::get<int>(meta.result->value);
    }
    EXPECT_EQ(byName["static_parent"], 10);
    EXPECT_EQ(byName["static_child_a"], 11);
    EXPECT_EQ(byName["static_child_b"], 12);
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

TEST(Scheduler, ScheduleAtPassesArgs) {
    ProcessPool pool(2);
    JobRegistry reg;
    reg.registerType("a", [] { return std::make_shared<JobA>(); });
    InMemoryExecutionRepository repo;
    Scheduler sched(pool, reg, repo, 1);
    sched.start();
    sched.scheduleAt("a", std::chrono::system_clock::from_time_t(0), {{"value", 99}});
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    auto jobs = sched.listJobs();
    ASSERT_EQ(jobs.size(), 1u);
    EXPECT_EQ(jobs[0].status, JobStatus::Completed);
    ASSERT_TRUE(jobs[0].result.has_value());
    EXPECT_EQ(std::get<int>(jobs[0].result->value), 99);
    sched.stop();
}

TEST(Scheduler, ScheduleEveryPassesArgs) {
    ProcessPool pool(2);
    JobRegistry reg;
    reg.registerType("a", [] { return std::make_shared<JobA>(); });
    InMemoryExecutionRepository repo;
    Scheduler sched(pool, reg, repo, 1);
    sched.start();
    sched.scheduleEvery("a", std::chrono::seconds(1), {{"value", 42}});
    std::this_thread::sleep_for(std::chrono::milliseconds(1500));
    auto jobs = sched.listJobs();
    ASSERT_EQ(jobs.size(), 1u);
    EXPECT_EQ(jobs[0].status, JobStatus::Completed);
    ASSERT_TRUE(jobs[0].result.has_value());
    EXPECT_EQ(std::get<int>(jobs[0].result->value), 42);
    sched.stop();
}

TEST(Scheduler, ScheduleCronValidDoesNotThrow) {
    ProcessPool pool(1);
    JobRegistry reg;
    reg.registerType("dummy", [] { return std::make_shared<DummyJob>(); });
    InMemoryExecutionRepository repo;
    Scheduler sched(pool, reg, repo, 1);
    sched.start();
    EXPECT_NO_THROW(sched.scheduleCron("*/1 * * * *", "dummy", {}));
    EXPECT_NO_THROW(sched.scheduleCron("*/15 * * * *", "dummy", {{"k", "v"}}));
    sched.stop();
}

TEST(Scheduler, ScheduleCronInvalidThrows) {
    ProcessPool pool(1);
    JobRegistry reg;
    reg.registerType("dummy", [] { return std::make_shared<DummyJob>(); });
    InMemoryExecutionRepository repo;
    Scheduler sched(pool, reg, repo, 1);
    EXPECT_THROW(sched.scheduleCron("0 * * * *", "dummy", {}), std::invalid_argument);
}
