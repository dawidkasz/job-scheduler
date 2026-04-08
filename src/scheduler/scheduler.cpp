#include "scheduler/scheduler.hpp"

#include <algorithm>
#include <regex>
#include <stdexcept>

#include "core/job_context.hpp"


Scheduler::Scheduler(ProcessPool& pool, JobRegistry& registry,
                     ExecutionRepository& repo, int maxRetries)
    : pool_(pool), registry_(registry),
      executionRepository_(repo), maxRetries_(maxRetries) {}

Scheduler::~Scheduler() { stop(); }

void Scheduler::start() {
    running_ = true;
    dispatchThread_ = std::thread(&Scheduler::dispatchLoop, this);
}

void Scheduler::stop() {
    if (!running_) return;
    running_ = false;
    pendingQueue_.push(nullptr);
    if (dispatchThread_.joinable())
        dispatchThread_.join();
}

JobId Scheduler::startJobByName(const std::string& jobName, nlohmann::json args, int priority) {
    auto job = registry_.create(jobName);
    auto exec = std::make_shared<JobExecution>(job, priority,
                                               std::chrono::system_clock::now(),
                                               std::move(args));
    executionRepository_.add(exec);
    pendingQueue_.push(exec);
    return exec->getId();
}

JobId Scheduler::startJobByName(const std::string& jobName, std::vector<JobId> after,
                                nlohmann::json args, int priority) {
    auto job = registry_.create(jobName);
    auto exec = std::make_shared<JobExecution>(job, priority,
                                               std::chrono::system_clock::now(),
                                               std::move(args));
    auto newId = exec->getId();
    executionRepository_.add(exec);
    for (const auto& depId : after) {
        depGraph_.addDependency(depId, newId);
    }
    pendingQueue_.push(exec);
    return newId;
}

void Scheduler::cancelJob(const JobId& jobId) {
    auto exec = executionRepository_.get(jobId);
    if (exec) {
        exec->setStatus(JobStatus::Cancelled);
        pendingQueue_.remove(jobId);
    }
}

std::optional<JobMetadata> Scheduler::getJobById(const JobId& jobId) const {
    auto exec = executionRepository_.get(jobId);
    if (!exec) return std::nullopt;
    return JobMetadata{exec->getId(), exec->getJobName(), exec->getPriority(),
                       exec->getStatus(), exec->getRetryCount(), exec->getCreatedAt(),
                       exec->getResult()};
}

std::vector<JobMetadata> Scheduler::listJobs() const {
    auto all = executionRepository_.getAll();
    std::vector<JobMetadata> result;
    result.reserve(all.size());
    for (const auto& exec : all) {
        result.push_back({exec->getId(), exec->getJobName(), exec->getPriority(),
                          exec->getStatus(), exec->getRetryCount(), exec->getCreatedAt(),
                          exec->getResult()});
    }
    return result;
}

void Scheduler::scheduleAt(const std::string& jobName,
                           std::chrono::system_clock::time_point when) {
    std::thread([this, jobName, when] {
        std::this_thread::sleep_until(when);
        if (running_) {
            startJobByName(jobName);
        }
    }).detach();
}

void Scheduler::scheduleEvery(const std::string& jobName, std::chrono::seconds interval) {
    std::thread([this, jobName, interval] {
        while (running_) {
            std::this_thread::sleep_for(interval);
            if (running_) {
                startJobByName(jobName);
            }
        }
    }).detach();
}

void Scheduler::scheduleCron(const std::string& cronExpr, const std::string& jobName) {
    std::regex pattern(R"(\*/(\d+)\s+\*\s+\*\s+\*\s+\*)");
    std::smatch match;
    if (!std::regex_match(cronExpr, match, pattern)) {
        throw std::invalid_argument("Only */N * * * * cron expressions are supported");
    }
    int minutes = std::stoi(match[1]);
    scheduleEvery(jobName, std::chrono::seconds(minutes * 60));
}

void Scheduler::dispatchLoop() {
    while (running_) {
        auto exec = pendingQueue_.pop();
        if (!running_ || !exec) break;

        auto deps = depGraph_.getDependencies(exec->getId());
        bool ready = std::all_of(deps.begin(), deps.end(), [&](const JobId& dep) {
            auto depExec = executionRepository_.get(dep);
            return depExec && depExec->getStatus() == JobStatus::Completed;
        });

        if (!ready) {
            pendingQueue_.push(exec);
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            continue;
        }

        exec->setStatus(JobStatus::Running);
        JobContext ctx{};
        ctx.args = exec->getArgs();
        for (const auto& depId : deps) {
            auto depExec = executionRepository_.get(depId);
            if (depExec && depExec->getResult().has_value()) {
                ctx.dependencyResults[depExec->getJobName()] = *depExec->getResult();
            }
        }
        auto handle = pool_.spawn([exec, ctx] {
            return exec->getJob().execute(ctx);
        });
        auto result = pool_.readResult(handle);

        if (result.success) {
            exec->setResult(result);
            exec->setStatus(JobStatus::Completed);
        } else if (exec->getRetryCount() < maxRetries_) {
            exec->incrementRetry();
            exec->setStatus(JobStatus::Pending);
            pendingQueue_.push(exec);
        } else {
            exec->setResult(result);
            exec->setStatus(JobStatus::Failed);
        }
    }
}
