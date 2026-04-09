#include "scheduler/scheduler.hpp"

#include <algorithm>
#include <cerrno>
#include <poll.h>
#include <regex>
#include <stdexcept>
#include <thread>
#include <vector>

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
    return startJobByName(jobName, {}, std::move(args), priority);
}

JobId Scheduler::startJobByName(const std::string& jobName, std::vector<JobId> after,
                                nlohmann::json args, int priority) {
    auto job = registry_.create(jobName);
    auto exec = std::make_shared<JobExecution>(job, priority,
                                               std::chrono::system_clock::now(),
                                               std::move(args));
    const JobId newId = exec->getId();
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
                           std::chrono::system_clock::time_point when, nlohmann::json args) {
    std::thread([this, jobName, when, args = std::move(args)] {
        std::this_thread::sleep_until(when);
        if (running_) {
            startJobByName(jobName, nlohmann::json(args));
        }
    }).detach();
}

void Scheduler::scheduleEvery(const std::string& jobName, std::chrono::seconds interval,
                                nlohmann::json args) {
    std::thread([this, jobName, interval, args = std::move(args)] {
        while (running_) {
            std::this_thread::sleep_for(interval);
            if (running_) {
                startJobByName(jobName, nlohmann::json(args));
            }
        }
    }).detach();
}

void Scheduler::scheduleCron(const std::string& cronExpr, const std::string& jobName,
                             nlohmann::json args) {
    std::regex pattern(R"(\*/(\d+)\s+\*\s+\*\s+\*\s+\*)");
    std::smatch match;
    if (!std::regex_match(cronExpr, match, pattern)) {
        throw std::invalid_argument("Only */N * * * * cron expressions are supported");
    }
    int minutes = std::stoi(match[1]);
    scheduleEvery(jobName, std::chrono::seconds(minutes * 60), std::move(args));
}

void Scheduler::dispatchLoop() {
    struct InFlight {
        std::shared_ptr<JobExecution> exec;
        ProcessPool::ChildProcessHandle handle;
        std::string buf;
    };
    std::vector<InFlight> inFlight;

    auto applyResult = [&](const std::shared_ptr<JobExecution>& exec, const JobResult& result) {
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
    };

    auto depsReady = [&](const JobId& id) {
        const auto deps = depGraph_.getDependencies(id);
        return std::all_of(deps.begin(), deps.end(), [&](const JobId& dep) {
            const auto depExec = executionRepository_.get(dep);
            return depExec && depExec->getStatus() == JobStatus::Completed;
        });
    };

    auto buildCtx = [&](const std::shared_ptr<JobExecution>& exec) {
        JobContext ctx;
        ctx.args = exec->getArgs();
        for (const auto& depId : depGraph_.getDependencies(exec->getId())) {
            const auto depExec = executionRepository_.get(depId);
            if (depExec && depExec->getResult().has_value())
                ctx.dependencyResults[depExec->getJobName()] = *depExec->getResult();
        }
        return ctx;
    };

    for (;;) {
        if (!inFlight.empty()) {
            std::vector<struct pollfd> pfds(inFlight.size());
            for (std::size_t i = 0; i < inFlight.size(); ++i) {
                pfds[i].fd = inFlight[i].handle.resultFd;
                pfds[i].events = static_cast<short>(POLLIN | POLLHUP);
                pfds[i].revents = 0;
            }
            int pr = 0;
            do {
                pr = poll(pfds.data(), static_cast<nfds_t>(pfds.size()), 50);
            } while (pr < 0 && errno == EINTR);

            for (std::size_t i = 0; i < inFlight.size();) {
                JobResult r;
                if (pool_.tryDrain(inFlight[i].handle, inFlight[i].buf, r)) {
                    applyResult(inFlight[i].exec, r);
                    inFlight[i] = std::move(inFlight.back());
                    inFlight.pop_back();
                } else {
                    ++i;
                }
            }
        }

        if (!running_ && inFlight.empty()) {
            std::vector<std::shared_ptr<JobExecution>> held;
            while (auto o = pendingQueue_.tryPop()) {
                if (!*o) {
                    for (auto& h : held) pendingQueue_.push(h);
                    return;
                }
                held.push_back(std::move(*o));
            }
            for (auto& h : held) pendingQueue_.push(h);
            return;
        }

        if (running_) {
            std::vector<std::shared_ptr<JobExecution>> notReady;
            bool blockedOnDeps = false;

            while (auto opt = pendingQueue_.tryPop()) {
                std::shared_ptr<JobExecution> exec = std::move(*opt);
                if (!exec) {
                    pendingQueue_.push(nullptr);
                    running_ = false;
                    break;
                }
                if (exec->getStatus() == JobStatus::Cancelled) continue;

                if (!depsReady(exec->getId())) {
                    notReady.push_back(std::move(exec));
                    blockedOnDeps = true;
                    continue;
                }
                if (inFlight.size() >= pool_.maxProcesses()) {
                    notReady.push_back(std::move(exec));
                    continue;
                }
                exec->setStatus(JobStatus::Running);
                const JobContext ctx = buildCtx(exec);
                ProcessPool::ChildProcessHandle h;
                if (!pool_.trySpawn([exec, ctx]() { return exec->getJob().execute(ctx); }, h)) {
                    exec->setStatus(JobStatus::Pending);
                    notReady.push_back(std::move(exec));
                    continue;
                }
                inFlight.push_back({std::move(exec), std::move(h), {}});
            }
            for (auto& d : notReady) pendingQueue_.push(d);

            if (inFlight.empty() && blockedOnDeps) {
                std::this_thread::sleep_for(std::chrono::milliseconds(50));
            }
        }

        if (inFlight.empty() && pendingQueue_.empty() && running_) {
            std::shared_ptr<JobExecution> w = pendingQueue_.pop();
            if (!w) return;
            pendingQueue_.push(std::move(w));
        }
    }
}
