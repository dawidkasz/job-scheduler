#pragma once

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

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(JobResult, value, success, error)
