#include "scheduler/dependency_graph.hpp"

#include <algorithm>

void DependencyGraph::addDependency(const JobId& parentId, const JobId& childId) {
    dependents_[parentId].push_back(childId);
    dependencies_[childId].push_back(parentId);
}

void DependencyGraph::removeDependency(const JobId& parentId, const JobId& childId) {
    auto& deps = dependents_[parentId];
    deps.erase(std::remove(deps.begin(), deps.end(), childId), deps.end());

    auto& parents = dependencies_[childId];
    parents.erase(std::remove(parents.begin(), parents.end(), parentId), parents.end());
}

std::vector<JobId> DependencyGraph::getDependents(const JobId& jobId) const {
    auto it = dependents_.find(jobId);
    return it != dependents_.end() ? it->second : std::vector<JobId>{};
}

std::vector<JobId> DependencyGraph::getDependencies(const JobId& jobId) const {
    auto it = dependencies_.find(jobId);
    return it != dependencies_.end() ? it->second : std::vector<JobId>{};
}
