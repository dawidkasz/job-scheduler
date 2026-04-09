#pragma once

#include <atomic>
#include <chrono>
#include <nlohmann/json.hpp>
#include <optional>
#include <string>
#include <thread>
#include <vector>

#include "core/job_id.hpp"
#include "core/job_metadata.hpp"
#include "core/job_result.hpp"
#include "core/job_status.hpp"
#include "scheduler/dependency_graph.hpp"
#include "scheduler/job_queue.hpp"
#include "scheduler/process_pool.hpp"
#include "storage/execution_repository.hpp"
#include "storage/job_registry.hpp"

/// Owns the dispatch thread, pending queue, dep graph, and talks to pool + registry + execution store.
/// start/stop around the long-running loop; everything else is enqueue / query.
class Scheduler {
public:
    Scheduler(ProcessPool& pool, JobRegistry& registry, ExecutionRepository& executionRepository, int maxRetries = 3);
    ~Scheduler();

    JobId startJobByName(const std::string& jobName, nlohmann::json args = {}, int priority = 0);
    JobId startJobByName(const std::string& jobName, std::vector<JobId> after, nlohmann::json args = {},
                         int priority = 0);

    void scheduleAt(const std::string& jobName, std::chrono::system_clock::time_point when,
                    nlohmann::json args = {});
    void scheduleEvery(const std::string& jobName, std::chrono::seconds interval,
                       nlohmann::json args = {});
    void scheduleCron(const std::string& cronExpr, const std::string& jobName,
                      nlohmann::json args = {});

    void cancelJob(const JobId& id);
    std::optional<JobMetadata> getJobById(const JobId& id) const;
    std::vector<JobMetadata> listJobs() const;

    void start();
    void stop();

private:
    void dispatchLoop();

    ProcessPool& pool_;
    JobRegistry& registry_;
    ExecutionRepository& executionRepository_;
    int maxRetries_;
    DependencyGraph depGraph_;
    JobQueue pendingQueue_;
    std::thread dispatchThread_;
    std::atomic<bool> running_{false};
};
