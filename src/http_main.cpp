#include <memory>

#include "api/http_controller.hpp"
#include "core/example_jobs.hpp"
#include "scheduler/process_pool.hpp"
#include "scheduler/scheduler.hpp"
#include "storage/in_memory_execution_repository.hpp"
#include "storage/job_registry.hpp"

int main(int argc, char* argv[]) {
    (void)argc;
    (void)argv;

    ProcessPool pool(4);
    JobRegistry registry;
    registry.registerType("echo", [] { return std::make_shared<EchoJob>(); });
    registry.registerType("sleep", [] { return std::make_shared<SleepJob>(); });
    registry.registerType("fail", [] { return std::make_shared<FailJob>(); });
    InMemoryExecutionRepository repo;
    Scheduler scheduler(pool, registry, repo);
    scheduler.start();

    HttpController http(scheduler, 8080);
    http.start();

    scheduler.stop();
    return 0;
}
