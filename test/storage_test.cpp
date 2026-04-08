#include <gtest/gtest.h>

#include "storage/in_memory_execution_repository.hpp"
#include "storage/job_registry.hpp"
#include "test_helpers.hpp"

TEST(JobRegistry, CreateRegisteredType) {
    JobRegistry reg;
    reg.registerType("dummy", [] { return std::make_shared<DummyJob>("dummy"); });
    auto job = reg.create("dummy");
    EXPECT_EQ(job->getName(), "dummy");
}

TEST(JobRegistry, UnknownTypeThrows) {
    JobRegistry reg;
    EXPECT_THROW(reg.create("nope"), std::runtime_error);
}

TEST(JobRegistry, HasType) {
    JobRegistry reg;
    reg.registerType("dummy", [] { return std::make_shared<DummyJob>(); });
    EXPECT_TRUE(reg.hasType("dummy"));
    EXPECT_FALSE(reg.hasType("other"));
}

TEST(InMemoryRepo, AddAndGet) {
    InMemoryExecutionRepository repo;
    auto job = std::make_shared<DummyJob>();
    auto exec = std::make_shared<JobExecution>(job);
    auto id = exec->getId();

    repo.add(exec);

    EXPECT_TRUE(repo.contains(id));
    EXPECT_EQ(repo.get(id)->getId(), id);
}

TEST(InMemoryRepo, RemoveDeletesEntry) {
    InMemoryExecutionRepository repo;
    auto exec = std::make_shared<JobExecution>(std::make_shared<DummyJob>());
    auto id = exec->getId();
    repo.add(exec);
    repo.remove(id);
    EXPECT_FALSE(repo.contains(id));
}

TEST(InMemoryRepo, GetAllReturnsAll) {
    InMemoryExecutionRepository repo;
    repo.add(std::make_shared<JobExecution>(std::make_shared<DummyJob>()));
    repo.add(std::make_shared<JobExecution>(std::make_shared<DummyJob>()));
    EXPECT_EQ(repo.getAll().size(), 2);
}
