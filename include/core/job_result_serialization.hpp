#pragma once

#include <optional>

#include <nlohmann/json.hpp>

#include "core/job_result.hpp"

inline void to_json(nlohmann::json& j, const JobResult::Payload& payload) {
    std::visit(
        [&j](auto&& val) {
            using T = std::decay_t<decltype(val)>;
            if constexpr (std::is_same_v<T, std::monostate>) {
                j = nullptr;
            } else {
                j = val;
            }
        },
        payload);
}

inline void from_json(const nlohmann::json& j, JobResult::Payload& payload) {
    if (j.is_null()) {
        payload = std::monostate{};
    } else if (j.is_string()) {
        payload = j.get<std::string>();
    } else if (j.is_number_integer()) {
        payload = j.get<int>();
    } else if (j.is_number_float()) {
        payload = j.get<double>();
    } else {
        payload = std::monostate{};
    }
}

inline void to_json(nlohmann::json& j, const JobResult& r) {
    j["success"] = r.success;
    nlohmann::json val;
    to_json(val, r.value);
    j["value"] = std::move(val);
    j["error"] = r.error ? nlohmann::json(*r.error) : nullptr;
}

inline void from_json(const nlohmann::json& j, JobResult& r) {
    r.success = j.at("success").get<bool>();
    from_json(j.at("value"), r.value);
    if (j.contains("error") && !j["error"].is_null())
        r.error = j["error"].get<std::string>();
    else
        r.error = std::nullopt;
}

inline nlohmann::json jobResultToJson(const JobResult& r) {
    nlohmann::json j;
    to_json(j, r);
    return j;
}

inline nlohmann::json optionalJobResultToJson(const std::optional<JobResult>& r) {
    if (!r) return nullptr;
    return jobResultToJson(*r);
}
