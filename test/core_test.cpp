#include <gtest/gtest.h>

#include "core/job_execution.hpp"
#include "core/job_id.hpp"
#include "core/job_result.hpp"
#include "core/job_status.hpp"
#include "test_helpers.hpp"

TEST(JobId, GenerateIsUnique) {
    auto a = JobId::generate();
    auto b = JobId::generate();
    EXPECT_NE(a, b);
}

TEST(JobId, FromStringRoundTrip) {
    auto id = JobId::generate();
    auto copy = JobId::fromString(id.str());
    EXPECT_EQ(id, copy);
}

TEST(JobId, FromStringThrowsOnInvalid) {
    EXPECT_THROW(JobId::fromString("not-a-uuid"), std::invalid_argument);
}

TEST(JobResult, OkHasSuccessTrue) {
    auto r = JobResult::ok(42);
    EXPECT_TRUE(r.success);
    EXPECT_FALSE(r.error.has_value());
}

TEST(JobResult, FailHasSuccessFalse) {
    auto r = JobResult::fail("oops");
    EXPECT_FALSE(r.success);
    EXPECT_EQ(r.error, "oops");
}

TEST(JobStatus, ToStringAllValues) {
    EXPECT_STREQ(jobStatusToString(JobStatus::Pending), "Pending");
    EXPECT_STREQ(jobStatusToString(JobStatus::Running), "Running");
    EXPECT_STREQ(jobStatusToString(JobStatus::Completed), "Completed");
    EXPECT_STREQ(jobStatusToString(JobStatus::Failed), "Failed");
    EXPECT_STREQ(jobStatusToString(JobStatus::Cancelled), "Cancelled");
}

TEST(JobExecution, InitialStatePending) {
    auto job = std::make_shared<DummyJob>();
    JobExecution exec(job);
    EXPECT_EQ(exec.getStatus(), JobStatus::Pending);
    EXPECT_EQ(exec.getRetryCount(), 0);
    EXPECT_EQ(exec.getPriority(), 0);
}
