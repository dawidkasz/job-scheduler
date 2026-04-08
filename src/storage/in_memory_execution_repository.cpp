#include "storage/in_memory_execution_repository.hpp"

#include <mutex>

void InMemoryExecutionRepository::add(std::shared_ptr<JobExecution> exec) {
    std::unique_lock lock(mutex_);
    executions_[exec->getId()] = std::move(exec);
}

std::shared_ptr<JobExecution> InMemoryExecutionRepository::get(const JobId& id) const {
    std::shared_lock lock(mutex_);
    auto it = executions_.find(id);
    return it != executions_.end() ? it->second : nullptr;
}

void InMemoryExecutionRepository::remove(const JobId& id) {
    std::unique_lock lock(mutex_);
    executions_.erase(id);
}

std::vector<std::shared_ptr<JobExecution>> InMemoryExecutionRepository::getAll() const {
    std::shared_lock lock(mutex_);
    std::vector<std::shared_ptr<JobExecution>> result;
    result.reserve(executions_.size());
    for (const auto& [_, exec] : executions_)
        result.push_back(exec);
    return result;
}

bool InMemoryExecutionRepository::contains(const JobId& id) const {
    std::shared_lock lock(mutex_);
    return executions_.count(id) > 0;
}
