#include "storage/job_registry.hpp"

#include <stdexcept>

void JobRegistry::registerType(const std::string& name, JobBlueprint creator) {
    blueprints_[name] = std::move(creator);
}

std::shared_ptr<Job> JobRegistry::create(const std::string& name) const {
    auto it = blueprints_.find(name);
    if (it == blueprints_.end())
        throw std::runtime_error("Unknown job type: " + name);
    return it->second();
}

bool JobRegistry::hasType(const std::string& name) const {
    return blueprints_.count(name) > 0;
}

std::vector<std::string> JobRegistry::registeredTypes() const {
    std::vector<std::string> names;
    names.reserve(blueprints_.size());
    for (const auto& [name, _] : blueprints_)
        names.push_back(name);
    return names;
}
