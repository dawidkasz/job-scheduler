#include <memory>

#include "api/http_controller.hpp"
#include "scheduler/process_pool.hpp"
#include "scheduler/scheduler.hpp"
#include "storage/in_memory_execution_repository.hpp"
#include "storage/job_registry.hpp"

#include "nlp_jobs.hpp"

int main() {
    ProcessPool pool(4);

    JobRegistry registry;
    registry.registerType("tokenize", [] { return std::make_shared<TokenizeJob>(); });
    registry.registerType("lowercase", [] { return std::make_shared<LowercaseJob>(); });
    registry.registerType("count_words", [] { return std::make_shared<CountWordsJob>(); });
    registry.registerType("format_report", [] { return std::make_shared<FormatReportJob>(); });

    InMemoryExecutionRepository repo;
    Scheduler scheduler(pool, registry, repo);
    scheduler.start();

    HttpController http(scheduler, 8080);
    http.start();

    scheduler.stop();
    return 0;
}
