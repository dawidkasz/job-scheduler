#pragma once

#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "core/job_id.hpp"

/// Parent/child edges only — Scheduler decides when dependents unblock.
class DependencyGraph {
public:
    void addDependency(const JobId& parentId, const JobId& childId);
    void removeDependency(const JobId& parentId, const JobId& childId);

    std::vector<JobId> getDependents(const JobId& jobId) const;
    std::vector<JobId> getDependencies(const JobId& jobId) const;

private:
    std::unordered_map<JobId, std::vector<JobId>> dependents_;
    std::unordered_map<JobId, std::vector<JobId>> dependencies_;
};
