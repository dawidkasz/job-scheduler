#pragma once

class Scheduler;

class CliController {
public:
    explicit CliController(Scheduler& scheduler);

    int run(int argc, char* argv[]);

private:
    Scheduler& scheduler_;
};
