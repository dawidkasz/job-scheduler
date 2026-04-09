#pragma once

/// Pending -> Running -> terminal (Completed / Failed / Cancelled).
enum class JobStatus {
    Pending,
    Running,
    Completed,
    Failed,
    Cancelled,
};

const char *jobStatusToString(JobStatus status);
