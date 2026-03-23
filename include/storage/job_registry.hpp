#pragma once

#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "core/job.hpp"

class JobRegistry {
public:
    using JobBlueprint = std::function<std::shared_ptr<Job>()>;

    void registerType(const std::string& name, JobBlueprint creator);
    std::shared_ptr<Job> create(const std::string& name) const;
    bool hasType(const std::string& name) const;
    std::vector<std::string> registeredTypes() const;

private:
    std::unordered_map<std::string, JobBlueprint> blueprints_;
};
