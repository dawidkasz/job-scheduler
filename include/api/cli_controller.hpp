#pragma once

class Scheduler;

/// program_options wrapper around the same scheduler ops the server exposes
class CliController {
public:
    explicit CliController(Scheduler& scheduler);

    int run();

private:
    int dispatchCommand(int argc, char* argv[]);

    Scheduler& scheduler_;
};
