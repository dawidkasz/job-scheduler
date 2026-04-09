#pragma once

/// @file ride_jobs.hpp
/// @brief Uber-like ride service pipeline with a GPS sensor stub.
///
/// Example usage scenario:
///   A vehicle periodically reports its GPS position.  FetchGPSData is a stub
///   that produces random coordinates near New York City, simulating a real GPS
///   sensor.  Two downstream jobs run in parallel: EstimateETA computes a rough
///   arrival time to the rider's destination, and CalculateSurge derives a
///   demand-based price multiplier from the vehicle's zone.  Finally,
///   UpdateRideStatus merges both results into a single rider-facing status
///   string.
///
///   DAG topology (diamond):
///
///          FetchGPSData   (periodic stub – random lat,lng)
///           /          \
///     EstimateETA   CalculateSurge
///           \          /
///        UpdateRideStatus
///
///   curl demo:
///     # 1. start the example server (./ride_service_example)
///     # 2. run ./demo.sh
///
///   The demo script also shows how to schedule FetchGPSData as a periodic
///   job (kind: "every") so it fires automatically every N seconds.

#include <chrono>
#include <cmath>
#include <random>
#include <sstream>
#include <string>
#include <thread>

#include "core/job.hpp"
#include "core/job_result.hpp"

namespace {
void simulateProcessing(int seconds = 3) {
    std::this_thread::sleep_for(std::chrono::seconds(seconds));
}
} // namespace

/// Stub: produces random GPS coordinates near NYC (40.7128 N, 74.0060 W).
/// In production this would read from a real vehicle GPS sensor.
class FetchGPSDataJob : public Job {
public:
    FetchGPSDataJob() : Job("fetch_gps_data") {}

    JobResult execute(const JobContext& /*ctx*/) override {
        simulateProcessing();
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_real_distribution<double> latDist(40.700, 40.730);
        std::uniform_real_distribution<double> lngDist(-74.020, -73.990);

        double lat = latDist(gen);
        double lng = lngDist(gen);

        std::ostringstream out;
        out.precision(6);
        out << std::fixed << lat << "," << lng;
        return JobResult::ok(out.str());
    }
};

namespace detail {

inline std::pair<double, double> parseLatLng(const std::string& s) {
    auto comma = s.find(',');
    double lat = std::stod(s.substr(0, comma));
    double lng = std::stod(s.substr(comma + 1));
    return {lat, lng};
}

inline double haversineKm(double lat1, double lng1, double lat2, double lng2) {
    constexpr double kEarthRadiusKm = 6371.0;
    constexpr double kDeg2Rad = M_PI / 180.0;
    double dLat = (lat2 - lat1) * kDeg2Rad;
    double dLng = (lng2 - lng1) * kDeg2Rad;
    double a = std::sin(dLat / 2) * std::sin(dLat / 2) +
               std::cos(lat1 * kDeg2Rad) * std::cos(lat2 * kDeg2Rad) *
               std::sin(dLng / 2) * std::sin(dLng / 2);
    return kEarthRadiusKm * 2.0 * std::atan2(std::sqrt(a), std::sqrt(1.0 - a));
}

} // namespace detail

/// Estimates arrival time (minutes) from the vehicle's GPS position to a
/// destination supplied via args["dest_lat"] / args["dest_lng"].
/// Assumes an average city speed of 25 km/h.
class EstimateETAJob : public Job {
public:
    EstimateETAJob() : Job("estimate_eta") {}

    JobResult execute(const JobContext& ctx) override {
        simulateProcessing();
        auto it = ctx.dependencyResults.find("fetch_gps_data");
        if (it == ctx.dependencyResults.end())
            return JobResult::fail("missing dependency: fetch_gps_data");

        auto [lat, lng] = detail::parseLatLng(std::get<std::string>(it->second.value));

        double destLat = ctx.args.value("dest_lat", 40.7580);
        double destLng = ctx.args.value("dest_lng", -73.9855);

        double distKm = detail::haversineKm(lat, lng, destLat, destLng);
        constexpr double kCitySpeedKmH = 25.0;
        int etaMinutes = std::max(1, static_cast<int>(std::round(distKm / kCitySpeedKmH * 60.0)));
        return JobResult::ok(etaMinutes);
    }
};

/// Derives a surge-pricing multiplier based on the vehicle's zone.
/// Midtown (lat > 40.715) gets a higher surge.
class CalculateSurgePricingJob : public Job {
public:
    CalculateSurgePricingJob() : Job("calculate_surge") {}

    JobResult execute(const JobContext& ctx) override {
        simulateProcessing();
        auto it = ctx.dependencyResults.find("fetch_gps_data");
        if (it == ctx.dependencyResults.end())
            return JobResult::fail("missing dependency: fetch_gps_data");

        auto [lat, lng] = detail::parseLatLng(std::get<std::string>(it->second.value));
        (void)lng;

        double surge = (lat > 40.715) ? 1.8 : 1.2;
        return JobResult::ok(surge);
    }
};

/// Combines ETA and surge pricing into a rider-facing status string.
class UpdateRideStatusJob : public Job {
public:
    UpdateRideStatusJob() : Job("update_ride_status") {}

    JobResult execute(const JobContext& ctx) override {
        simulateProcessing(5);
        auto etaIt = ctx.dependencyResults.find("estimate_eta");
        auto surgeIt = ctx.dependencyResults.find("calculate_surge");
        if (etaIt == ctx.dependencyResults.end() || surgeIt == ctx.dependencyResults.end())
            return JobResult::fail("missing dependencies");

        int eta = std::get<int>(etaIt->second.value);
        double surge = std::get<double>(surgeIt->second.value);
        double baseFare = 12.50;
        double fare = baseFare * surge;

        std::ostringstream out;
        out.precision(2);
        out << std::fixed
            << "ETA: " << eta << " min | Surge: " << surge << "x | Fare: $" << fare;
        return JobResult::ok(out.str());
    }
};
