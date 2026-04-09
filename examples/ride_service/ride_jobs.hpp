#pragma once

/// Fake GPS stub -> ETA vs surge in parallel -> UpdateRideStatus. Same diamond shape as the NLP example.

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

/// random lat,lng around NYC — stand-in for a real sensor
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

/** ETA minutes from gps string; dest_* in args; ~25km/h fudge */
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

/// surge multiplier from lat (midtown heuristic)
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

/// eta + surge -> status string with a fake fare
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
