#include "api/http_controller.hpp"

#include <array>
#include <algorithm>
#include <chrono>
#include <ctime>
#include <sstream>
#include <stdexcept>
#include <string>

#include <boost/asio.hpp>
#include <nlohmann/json.hpp>

#include "core/job_id.hpp"
#include "core/job_result_serialization.hpp"
#include "core/job_status.hpp"
#include "scheduler/scheduler.hpp"

using boost::asio::ip::tcp;

namespace {

static boost::asio::io_context* activeIoc = nullptr;

std::string makeResponse(int code, const std::string& body) {
    std::string status;
    if (code == 200) status = "200 OK";
    else if (code == 404) status = "404 Not Found";
    else status = "400 Bad Request";

    std::ostringstream oss;
    oss << "HTTP/1.1 " << status << "\r\n"
        << "Content-Type: application/json\r\n"
        << "Content-Length: " << body.size() << "\r\n"
        << "Connection: close\r\n"
        << "\r\n"
        << body;
    return oss.str();
}

void handleRequest(tcp::socket& sock, Scheduler& scheduler) {
    boost::asio::streambuf buf;
    boost::system::error_code ec;
    boost::asio::read_until(sock, buf, "\r\n\r\n", ec);
    if (ec) return;

    std::string raw(boost::asio::buffers_begin(buf.data()),
                    boost::asio::buffers_end(buf.data()));
    buf.consume(buf.size());

    auto headerEnd = raw.find("\r\n\r\n");
    if (headerEnd == std::string::npos) return;

    std::string headers = raw.substr(0, headerEnd);
    std::string body = raw.substr(headerEnd + 4);

    std::string requestLine = headers.substr(0, headers.find("\r\n"));
    std::string method;
    std::string path;
    std::istringstream(requestLine) >> method >> path;

    std::size_t contentLength = 0;
    auto clPos = headers.find("Content-Length:");
    if (clPos == std::string::npos)
        clPos = headers.find("content-length:");
    if (clPos != std::string::npos) {
        auto colon = headers.find(':', clPos);
        if (colon != std::string::npos)
            contentLength = static_cast<std::size_t>(
                std::stoul(headers.substr(colon + 1)));
    }

    while (body.size() < contentLength) {
        std::array<char, 4096> chunk{};
        std::size_t need = contentLength - body.size();
        std::size_t toRead = std::min(need, chunk.size());
        std::size_t n = sock.read_some(boost::asio::buffer(chunk.data(), toRead), ec);
        if (ec || n == 0) break;
        body.append(chunk.data(), n);
    }
    if (body.size() > contentLength)
        body.resize(contentLength);

    std::string response;
    try {
        if (method == "POST" && path == "/jobs") {
            auto parsed = nlohmann::json::parse(body);
            if (!parsed.contains("kind") || !parsed["kind"].is_string()) {
                response = makeResponse(400, R"({"error":"kind required"})");
            } else if (!parsed.contains("name") || !parsed["name"].is_string()) {
                response = makeResponse(400, R"({"error":"name required"})");
            } else if (!parsed.contains("args") || !parsed["args"].is_object()) {
                response = makeResponse(400, R"({"error":"args object required"})");
            } else {
                const std::string kind = parsed["kind"].get<std::string>();
                const std::string name = parsed["name"].get<std::string>();
                nlohmann::json jobArgs = parsed["args"];
                if (kind == "run") {
                    auto jobId = scheduler.startJobByName(name, std::move(jobArgs));
                    response = makeResponse(200, nlohmann::json{{"id", jobId.str()}}.dump());
                } else if (kind == "at") {
                    if (!parsed.contains("runAtEpoch") || !parsed["runAtEpoch"].is_number_integer()) {
                        response = makeResponse(400, R"({"error":"runAtEpoch required"})");
                    } else {
                        const auto epoch = parsed["runAtEpoch"].get<std::int64_t>();
                        auto when = std::chrono::system_clock::from_time_t(
                            static_cast<std::time_t>(epoch));
                        scheduler.scheduleAt(name, when, std::move(jobArgs));
                        response = makeResponse(200,
                            nlohmann::json{{"scheduled", true}, {"kind", "at"}}.dump());
                    }
                } else if (kind == "every") {
                    if (!parsed.contains("everySeconds") || !parsed["everySeconds"].is_number_integer()) {
                        response = makeResponse(400, R"({"error":"everySeconds required"})");
                    } else {
                        const auto sec = parsed["everySeconds"].get<std::int64_t>();
                        if (sec <= 0) {
                            response = makeResponse(400, R"({"error":"everySeconds must be positive"})");
                        } else {
                            scheduler.scheduleEvery(name, std::chrono::seconds(sec), std::move(jobArgs));
                            response = makeResponse(200,
                                nlohmann::json{{"scheduled", true}, {"kind", "every"}}.dump());
                        }
                    }
                } else if (kind == "cron") {
                    if (!parsed.contains("cron") || !parsed["cron"].is_string()) {
                        response = makeResponse(400, R"({"error":"cron required"})");
                    } else {
                        try {
                            scheduler.scheduleCron(parsed["cron"].get<std::string>(), name,
                                                   std::move(jobArgs));
                            response = makeResponse(200,
                                nlohmann::json{{"scheduled", true}, {"kind", "cron"}}.dump());
                        } catch (const std::invalid_argument& e) {
                            response = makeResponse(400,
                                nlohmann::json{{"error", e.what()}}.dump());
                        }
                    }
                } else {
                    response = makeResponse(400, R"({"error":"unknown kind"})");
                }
            }

        } else if (method == "GET" && path == "/jobs") {
            nlohmann::json arr = nlohmann::json::array();
            for (const auto& meta : scheduler.listJobs()) {
                arr.push_back({{"id", meta.id.str()},
                               {"name", meta.jobName},
                               {"status", jobStatusToString(meta.status)},
                               {"retryCount", meta.retryCount},
                               {"result", optionalJobResultToJson(meta.result)}});
            }
            response = makeResponse(200, arr.dump());

        } else if (method == "GET" && path.rfind("/jobs/", 0) == 0) {
            auto meta = scheduler.getJobById(JobId::fromString(path.substr(6)));
            if (!meta) {
                response = makeResponse(404, R"({"error":"not found"})");
            } else {
                nlohmann::json j = {{"id", meta->id.str()},
                                    {"name", meta->jobName},
                                    {"status", jobStatusToString(meta->status)},
                                    {"retryCount", meta->retryCount},
                                    {"result", optionalJobResultToJson(meta->result)}};
                response = makeResponse(200, j.dump());
            }

        } else if (method == "DELETE" && path.rfind("/jobs/", 0) == 0) {
            scheduler.cancelJob(JobId::fromString(path.substr(6)));
            response = makeResponse(200, "{}");

        } else {
            response = makeResponse(404, R"({"error":"not found"})");
        }
    } catch (...) {
        response = makeResponse(400, R"({"error":"bad request"})");
    }

    boost::asio::write(sock, boost::asio::buffer(response), ec);
}

}

HttpController::HttpController(Scheduler& scheduler, std::uint16_t port)
    : scheduler_(scheduler), port_(port) {}

void HttpController::start() {
    boost::asio::io_context ioc;
    activeIoc = &ioc;
    tcp::acceptor acceptor(ioc, tcp::endpoint(tcp::v4(), port_));

    while (true) {
        tcp::socket sock(ioc);
        boost::system::error_code ec;
        acceptor.accept(sock, ec);
        if (ec) break;
        handleRequest(sock, scheduler_);
        sock.close();
    }

    activeIoc = nullptr;
}

void HttpController::stop() {
    if (activeIoc)
        activeIoc->stop();
}
