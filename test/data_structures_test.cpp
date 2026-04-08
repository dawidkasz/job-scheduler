#include <gtest/gtest.h>

#include "scheduler/dependency_graph.hpp"
#include "scheduler/job_queue.hpp"
#include "test_helpers.hpp"

TEST(DependencyGraph, AddAndQuery) {
    DependencyGraph g;
    auto a = JobId::generate();
    auto b = JobId::generate();
    g.addDependency(a, b);
    EXPECT_EQ(g.getDependents(a).size(), 1u);
    EXPECT_EQ(g.getDependencies(b).size(), 1u);
}

TEST(DependencyGraph, RemoveDependency) {
    DependencyGraph g;
    auto a = JobId::generate();
    auto b = JobId::generate();
    g.addDependency(a, b);
    g.removeDependency(a, b);
    EXPECT_EQ(g.getDependents(a).size(), 0u);
    EXPECT_EQ(g.getDependencies(b).size(), 0u);
}

TEST(JobQueue, PopsHighestPriorityFirst) {
    JobQueue q;
    auto now = std::chrono::system_clock::now();
    auto low = std::make_shared<JobExecution>(std::make_shared<DummyJob>(), 5, now);
    auto high = std::make_shared<JobExecution>(std::make_shared<DummyJob>(), 0, now);
    auto lowId = low->getId();
    auto highId = high->getId();

    q.push(low);
    q.push(high);

    EXPECT_EQ(q.pop()->getId(), highId);
    EXPECT_EQ(q.pop()->getId(), lowId);
}

TEST(JobQueue, RemoveById) {
    JobQueue q;
    auto exec = std::make_shared<JobExecution>(std::make_shared<DummyJob>());
    auto id = exec->getId();
    q.push(exec);
    EXPECT_TRUE(q.remove(id));
    EXPECT_TRUE(q.empty());
}
