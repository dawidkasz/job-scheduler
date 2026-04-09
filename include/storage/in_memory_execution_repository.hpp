#pragma once

#include <memory>
#include <shared_mutex>
#include <unordered_map>
#include <vector>

#include "core/job_execution.hpp"
#include "core/job_id.hpp"
#include "storage/execution_repository.hpp"

/// Default: map under a shared_mutex, good enough for demos/tests.
class InMemoryExecutionRepository : public ExecutionRepository {
public:
    void add(std::shared_ptr<JobExecution> exec) override;
    std::shared_ptr<JobExecution> get(const JobId& id) const override;
    void remove(const JobId& id) override;
    std::vector<std::shared_ptr<JobExecution>> getAll() const override;
    bool contains(const JobId& id) const override;

private:
    mutable std::shared_mutex mutex_;
    std::unordered_map<JobId, std::shared_ptr<JobExecution>> executions_;
};
