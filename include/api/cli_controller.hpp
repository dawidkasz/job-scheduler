#pragma once

class Scheduler;

class CliController {
public:
    explicit CliController(Scheduler& scheduler);

    int run();

private:
    int dispatchCommand(int argc, char* argv[]);

    Scheduler& scheduler_;
};
