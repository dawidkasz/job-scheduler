#pragma once

#include <memory>
#include <vector>

#include "core/job_execution.hpp"
#include "core/job_id.hpp"

class ExecutionRepository {
public:
    virtual ~ExecutionRepository() = default;

    virtual void add(std::shared_ptr<JobExecution> exec) = 0;
    virtual std::shared_ptr<JobExecution> get(const JobId& id) const = 0;
    virtual void remove(const JobId& id) = 0;
    virtual std::vector<std::shared_ptr<JobExecution>> getAll() const = 0;
    virtual bool contains(const JobId& id) const = 0;
};
