#pragma once

#include <chrono>
#include <iomanip>
#include <random>
#include <sstream>
#include <string>
#include <thread>

#include "core/job.hpp"
#include "core/job_result.hpp"

namespace {
void simulateProcessing() {
    std::this_thread::sleep_for(std::chrono::milliseconds(300));
}
} // namespace

class FetchTemperatureSensorJob : public Job {
public:
    FetchTemperatureSensorJob() : Job("fetch_temperature_sensor") {}

    JobResult execute(const JobContext& ctx) override {
        simulateProcessing();
        std::string sensor = ctx.args.value("sensor", std::string("unknown"));

        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_real_distribution<double> tempC(18.0, 26.0);
        double t = tempC(gen);

        std::ostringstream out;
        out << std::fixed << std::setprecision(1);
        out << "temperature at sensor " << sensor << " is " << t;
        return JobResult::ok(out.str());
    }
};
