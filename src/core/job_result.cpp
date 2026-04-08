#include "core/job_result.hpp"

JobResult JobResult::ok(Payload val) {
    return JobResult{std::move(val), true, std::nullopt};
}

JobResult JobResult::fail(std::string errorMsg) {
    return JobResult{std::monostate{}, false, std::move(errorMsg)};
}
