#pragma once

enum class JobStatus {
    Pending,
    Running,
    Completed,
    Failed,
    Cancelled,
};

const char *jobStatusToString(JobStatus status);
