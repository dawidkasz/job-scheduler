#pragma once

#include <cstdint>

class Scheduler;

/// Minimal HTTP over asio/tcp; thin delegate to Scheduler (routes in cpp).
class HttpController {
public:
    HttpController(Scheduler& scheduler, std::uint16_t port = 8080);

    void start();
    void stop();

private:
    Scheduler& scheduler_;
    std::uint16_t port_;
};
