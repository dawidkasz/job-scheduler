#pragma once

#include <optional>
#include <string>
#include <variant>

struct JobResult {
    using Payload = std::variant<std::monostate, std::string, int, double>;

    Payload value;
    bool success{false};
    std::optional<std::string> error;

    static JobResult ok(Payload val = std::monostate{});
    static JobResult fail(std::string errorMsg);
};
