#include <memory>

#include "api/http_controller.hpp"
#include "scheduler/process_pool.hpp"
#include "scheduler/scheduler.hpp"
#include "storage/in_memory_execution_repository.hpp"
#include "storage/job_registry.hpp"

#include "ride_jobs.hpp"

int main() {
    ProcessPool pool(4);

    JobRegistry registry;
    registry.registerType("fetch_gps_data", [] { return std::make_shared<FetchGPSDataJob>(); });
    registry.registerType("estimate_eta", [] { return std::make_shared<EstimateETAJob>(); });
    registry.registerType("calculate_surge", [] { return std::make_shared<CalculateSurgePricingJob>(); });
    registry.registerType("update_ride_status", [] { return std::make_shared<UpdateRideStatusJob>(); });

    InMemoryExecutionRepository repo;
    Scheduler scheduler(pool, registry, repo);
    scheduler.start();

    HttpController http(scheduler, 8080);
    http.start();

    scheduler.stop();
    return 0;
}
