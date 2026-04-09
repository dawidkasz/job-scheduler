#include "api/cli_controller.hpp"

#include <iostream>
#include <sstream>
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
    std::cout << "Job scheduler. Commands: submit <name> [json-args] | list | status <id> | cancel <id> | help | quit\n";
    std::string line;
    while (std::cout << "> " && std::getline(std::cin, line)) {
        auto words = splitWords(line);
        if (words.empty()) continue;
        if (words[0] == "quit" || words[0] == "exit") break;
        if (words[0] == "help") {
            std::cout << "submit <name> [json-args]  e.g. submit echo {\"msg\":\"hi\"}\n"
                      << "list  status <id>  cancel <id>  quit\n";
            continue;
        }
        if (words[0] == "submit") {
            if (words.size() < 2) {
                std::cerr << "submit requires a job name\n";
                continue;
            }
            std::string jsonStr = words.size() > 2 ? joinFrom(2, words) : "{}";
            try {
                nlohmann::json args = nlohmann::json::parse(jsonStr);
                auto jobId = scheduler_.startJobByName(words[1], std::move(args));
                std::cout << jobId.str() << "\n";
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
