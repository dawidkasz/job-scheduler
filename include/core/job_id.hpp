#pragma once

#include <stdexcept>
#include <string>

class JobId {
public:
    static JobId generate();

    static JobId fromString(const std::string &str);

    const std::string &str() const { return value_; }

    bool operator==(const JobId &) const = default;

private:
    explicit JobId(std::string value) : value_(std::move(value)) {}

    static bool isValidUuid(const std::string &s);

    std::string value_;
};

template <>
struct std::hash<JobId> {
    std::size_t operator()(const JobId &id) const noexcept { return std::hash<std::string>{}(id.str()); }
};
