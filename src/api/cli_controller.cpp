#include "api/cli_controller.hpp"

#include <chrono>
#include <ctime>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#include <boost/program_options.hpp>
#include <nlohmann/json.hpp>

#include "core/job_id.hpp"
#include "core/job_result_serialization.hpp"
#include "core/job_status.hpp"
#include "scheduler/scheduler.hpp"

namespace po = boost::program_options;

CliController::CliController(Scheduler& scheduler) : scheduler_(scheduler) {}

int CliController::dispatchCommand(int argc, char* argv[]) {
    po::options_description desc("Options");
    desc.add_options()
        ("command", po::value<std::string>(), "list|status|cancel")
        ("arg", po::value<std::string>(), "job name or id");

    po::positional_options_description pos;
    pos.add("command", 1).add("arg", 1);

    po::variables_map vm;
    try {
        po::store(po::command_line_parser(argc, argv)
            .options(desc).positional(pos).run(), vm);
        po::notify(vm);
    } catch (const po::error& e) {
        std::cerr << e.what() << "\n";
        return 1;
    }

    if (!vm.count("command")) {
        std::cout << "Commands: list, status <id>, cancel <id> (type help for more)\n";
        return 0;
    }

    std::string cmd = vm["command"].as<std::string>();

    if (cmd == "list") {
        for (const auto& meta : scheduler_.listJobs()) {
            std::cout << meta.id.str() << " " << meta.jobName << " "
                      << jobStatusToString(meta.status) << " "
                      << optionalJobResultToJson(meta.result).dump() << "\n";
        }
    } else if (cmd == "status") {
        if (!vm.count("arg")) { std::cerr << "status requires a job id\n"; return 1; }
        auto meta = scheduler_.getJobById(JobId::fromString(vm["arg"].as<std::string>()));
        if (!meta) { std::cout << "Job not found\n"; return 1; }
        std::cout << jobStatusToString(meta->status) << "\n"
                  << optionalJobResultToJson(meta->result).dump() << "\n";
    } else if (cmd == "cancel") {
        if (!vm.count("arg")) { std::cerr << "cancel requires a job id\n"; return 1; }
        scheduler_.cancelJob(JobId::fromString(vm["arg"].as<std::string>()));
        std::cout << "Cancelled\n";
    } else {
        std::cerr << "Unknown command: " << cmd << "\n";
        return 1;
    }

    return 0;
}

namespace {
std::vector<std::string> splitWords(const std::string& line) {
    std::istringstream iss(line);
    std::vector<std::string> words;
    std::string w;
    while (iss >> w) words.push_back(std::move(w));
    return words;
}

std::string joinFrom(std::size_t start, const std::vector<std::string>& words) {
    std::ostringstream oss;
    for (std::size_t i = start; i < words.size(); ++i) {
        if (i > start) oss << ' ';
        oss << words[i];
    }
    return oss.str();
}
}

int CliController::run() {
    std::cout << "Job scheduler. submit <name> [json-body] | schedule ... | list | status <id> | cancel <id> | help | quit\n";
    std::string line;
    while (std::cout << "> " && std::getline(std::cin, line)) {
        auto words = splitWords(line);
        if (words.empty()) continue;
        if (words[0] == "quit" || words[0] == "exit") break;
        if (words[0] == "help") {
            std::cout << "submit <name> {\"args\":{...} [, \"dependsOn\":[\"uuid\"]]}\n"
                      << "schedule at <runAtEpoch> <name> [json-args]\n"
                      << "schedule every <seconds> <name> [json-args]\n"
                      << "schedule cron <N> <name> [json-args]  (every N minutes, */N * * * *)\n"
                      << "list  status <id>  cancel <id>  quit\n";
            continue;
        }
        if (words[0] == "submit") {
            if (words.size() < 2) {
                std::cerr << "submit requires a job name\n";
                continue;
            }
            std::string jsonStr =
                words.size() > 2 ? joinFrom(2, words) : std::string(R"({"args":{}})");
            try {
                nlohmann::json parsed = nlohmann::json::parse(jsonStr);
                if (!parsed.is_object() || !parsed.contains("args") ||
                    !parsed["args"].is_object()) {
                    std::cerr << "submit: expected {\"args\":{...}} with optional dependsOn\n";
                    continue;
                }
                std::vector<JobId> deps;
                if (parsed.contains("dependsOn") && !parsed["dependsOn"].is_null()) {
                    const auto& depJson = parsed["dependsOn"];
                    if (!depJson.is_array()) {
                        throw std::invalid_argument(
                            "dependsOn must be a JSON array of UUID strings");
                    }
                    for (const auto& el : depJson) {
                        if (!el.is_string()) {
                            throw std::invalid_argument(
                                "dependsOn entries must be UUID strings");
                        }
                        deps.push_back(JobId::fromString(el.get<std::string>()));
                    }
                }
                nlohmann::json jobArgs = std::move(parsed["args"]);
                bool unknownDep = false;
                for (const auto& depId : deps) {
                    if (!scheduler_.getJobById(depId)) {
                        unknownDep = true;
                        break;
                    }
                }
                if (unknownDep) {
                    std::cerr << "unknown dependency id\n";
                    continue;
                }
                auto jobId = scheduler_.startJobByName(words[1], std::move(deps), std::move(jobArgs));
                std::cout << jobId.str() << "\n";
            } catch (const std::invalid_argument& e) {
                std::cerr << "submit: " << e.what() << "\n";
            } catch (const std::exception& e) {
                std::cerr << "invalid args JSON: " << e.what() << "\n";
            }
            continue;
        }
        if (words[0] == "schedule") {
            if (words.size() < 2) {
                std::cerr << "schedule requires: at | every | cron\n";
                continue;
            }
            const std::string& sub = words[1];
            try {
                if (sub == "at") {
                    if (words.size() < 4) {
                        std::cerr << "schedule at <runAtEpoch> <name> [json-args]\n";
                        continue;
                    }
                    const auto epoch = std::stoll(words[2]);
                    const std::string& jobName = words[3];
                    std::string jsonStr = words.size() > 4 ? joinFrom(4, words) : "{}";
                    nlohmann::json args = nlohmann::json::parse(jsonStr);
                    auto when = std::chrono::system_clock::from_time_t(static_cast<std::time_t>(epoch));
                    scheduler_.scheduleAt(jobName, when, std::move(args));
                    std::cout << "scheduled at\n";
                } else if (sub == "every") {
                    if (words.size() < 4) {
                        std::cerr << "schedule every <seconds> <name> [json-args]\n";
                        continue;
                    }
                    const auto sec = std::stoll(words[2]);
                    if (sec <= 0) {
                        std::cerr << "everySeconds must be positive\n";
                        continue;
                    }
                    const std::string& jobName = words[3];
                    std::string jsonStr = words.size() > 4 ? joinFrom(4, words) : "{}";
                    nlohmann::json args = nlohmann::json::parse(jsonStr);
                    scheduler_.scheduleEvery(jobName, std::chrono::seconds(sec), std::move(args));
                    std::cout << "scheduled every\n";
                } else if (sub == "cron") {
                    if (words.size() < 4) {
                        std::cerr << "schedule cron <N> <name> [json-args]\n";
                        continue;
                    }
                    const int n = std::stoi(words[2]);
                    if (n <= 0) {
                        std::cerr << "cron N must be positive\n";
                        continue;
                    }
                    const std::string& jobName = words[3];
                    std::string jsonStr = words.size() > 4 ? joinFrom(4, words) : "{}";
                    nlohmann::json args = nlohmann::json::parse(jsonStr);
                    const std::string cronExpr = "*/" + std::to_string(n) + " * * * *";
                    scheduler_.scheduleCron(cronExpr, jobName, std::move(args));
                    std::cout << "scheduled cron\n";
                } else {
                    std::cerr << "unknown schedule subcommand (at|every|cron)\n";
                }
            } catch (const std::invalid_argument& e) {
                std::cerr << "schedule: " << e.what() << "\n";
            } catch (const std::exception& e) {
                std::cerr << "invalid args JSON: " << e.what() << "\n";
            }
            continue;
        }
        std::vector<std::string> args;
        args.emplace_back("job_scheduler_cli");
        args.insert(args.end(), words.begin(), words.end());
        std::vector<char*> argv;
        for (auto& s : args) argv.push_back(s.data());
        dispatchCommand(static_cast<int>(argv.size()), argv.data());
    }
    return 0;
}
