#include <memory>

#include "api/http_controller.hpp"
#include "scheduler/process_pool.hpp"
#include "scheduler/scheduler.hpp"
#include "storage/in_memory_execution_repository.hpp"
#include "storage/job_registry.hpp"

#include "temperature_jobs.hpp"

int main() {
    ProcessPool pool(4);

    JobRegistry registry;
    registry.registerType("fetch_temperature_sensor",
                          [] { return std::make_shared<FetchTemperatureSensorJob>(); });

    InMemoryExecutionRepository repo;
    Scheduler scheduler(pool, registry, repo);
    scheduler.start();

    HttpController http(scheduler, 8080);
    http.start();

    scheduler.stop();
    return 0;
}
