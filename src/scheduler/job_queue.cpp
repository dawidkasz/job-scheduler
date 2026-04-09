#include "scheduler/job_queue.hpp"

namespace {
bool cmp(const std::shared_ptr<JobExecution>& a, const std::shared_ptr<JobExecution>& b) {
    if (!a) return true;
    if (!b) return false;
    if (a->getRunAt() != b->getRunAt())
        return a->getRunAt() > b->getRunAt();
    return a->getPriority() > b->getPriority();
}
}

void JobQueue::push(std::shared_ptr<JobExecution> exec) {
    std::lock_guard lock(mutex_);
    heap_.push_back(std::move(exec));
    std::push_heap(heap_.begin(), heap_.end(), cmp);
    cv_.notify_one();
}

std::shared_ptr<JobExecution> JobQueue::pop() {
    std::unique_lock lock(mutex_);
    cv_.wait(lock, [this] { return !heap_.empty(); });
    std::pop_heap(heap_.begin(), heap_.end(), cmp);
    auto top = std::move(heap_.back());
    heap_.pop_back();
    return top;
}

std::optional<std::shared_ptr<JobExecution>> JobQueue::tryPop() {
    std::lock_guard lock(mutex_);
    if (heap_.empty()) return std::nullopt;
    std::pop_heap(heap_.begin(), heap_.end(), cmp);
    auto top = std::move(heap_.back());
    heap_.pop_back();
    return top;
}

std::optional<std::shared_ptr<JobExecution>> JobQueue::popFor(std::chrono::milliseconds timeout) {
    std::unique_lock lock(mutex_);
    if (!cv_.wait_for(lock, timeout, [this] { return !heap_.empty(); })) return std::nullopt;
    std::pop_heap(heap_.begin(), heap_.end(), cmp);
    auto top = std::move(heap_.back());
    heap_.pop_back();
    return top;
}

bool JobQueue::remove(const JobId& id) {
    std::lock_guard lock(mutex_);
    auto it = std::find_if(heap_.begin(), heap_.end(),
                           [&id](const auto& e) { return e->getId() == id; });
    if (it == heap_.end()) return false;
    heap_.erase(it);
    std::make_heap(heap_.begin(), heap_.end(), cmp);
    return true;
}

std::size_t JobQueue::size() const {
    std::lock_guard lock(mutex_);
    return heap_.size();
}

bool JobQueue::empty() const {
    std::lock_guard lock(mutex_);
    return heap_.empty();
}
