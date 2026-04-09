#pragma once

/// @file nlp_jobs.hpp
/// @brief NLP text-processing pipeline as a diamond-shaped job DAG.
///
/// Example usage scenario:
///   A document arrives for processing.  The first stage (Tokenize) splits the
///   raw text into individual tokens.  Two independent analyses then run in
///   parallel: Lowercase normalises every token to lower-case, while CountWords
///   tallies the number of unique tokens.  Finally, FormatReport merges both
///   results into a human-readable summary.
///
///   DAG topology (diamond):
///
///            Tokenize
///           /        \
///      Lowercase   CountWords
///           \        /
///         FormatReport
///
///   curl demo:
///     # 1. start the example server (./nlp_example)
///     # 2. run ./demo.sh

#include <algorithm>
#include <cctype>
#include <chrono>
#include <set>
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

/// Splits args["text"] on whitespace and returns space-separated tokens.
class TokenizeJob : public Job {
public:
    TokenizeJob() : Job("tokenize") {}

    JobResult execute(const JobContext& ctx) override {
        simulateProcessing();
        std::string text = ctx.args.value("text", "");
        std::string token;
        std::istringstream stream(text);
        std::ostringstream out;

        bool first = true;
        while (stream >> token) {
            std::string cleaned;
            for (char ch : token) {
                if (std::isalnum(static_cast<unsigned char>(ch))) {
                    cleaned += ch;
                }
            }
            if (cleaned.empty()) continue;
            if (!first) out << ' ';
            out << cleaned;
            first = false;
        }
        return JobResult::ok(out.str());
    }
};

/// Lowercases every character in the tokenized string from Tokenize.
class LowercaseJob : public Job {
public:
    LowercaseJob() : Job("lowercase") {}

    JobResult execute(const JobContext& ctx) override {
        simulateProcessing();
        auto it = ctx.dependencyResults.find("tokenize");
        if (it == ctx.dependencyResults.end())
            return JobResult::fail("missing dependency: tokenize");

        std::string tokens = std::get<std::string>(it->second.value);
        std::transform(tokens.begin(), tokens.end(), tokens.begin(),
                       [](unsigned char c) { return std::tolower(c); });
        return JobResult::ok(tokens);
    }
};

/// Counts unique tokens in the tokenized string from Tokenize.
class CountWordsJob : public Job {
public:
    CountWordsJob() : Job("count_words") {}

    JobResult execute(const JobContext& ctx) override {
        simulateProcessing();
        auto it = ctx.dependencyResults.find("tokenize");
        if (it == ctx.dependencyResults.end())
            return JobResult::fail("missing dependency: tokenize");

        std::string tokens = std::get<std::string>(it->second.value);
        std::istringstream stream(tokens);
        std::set<std::string> unique;
        std::string word;
        while (stream >> word) {
            std::string lower;
            lower.reserve(word.size());
            for (char ch : word)
                lower += static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
            unique.insert(lower);
        }
        return JobResult::ok(static_cast<int>(unique.size()));
    }
};

/// Merges the Lowercase text and CountWords count into a summary string.
class FormatReportJob : public Job {
public:
    FormatReportJob() : Job("format_report") {}

    JobResult execute(const JobContext& ctx) override {
        simulateProcessing(5);
        auto lcIt = ctx.dependencyResults.find("lowercase");
        auto cwIt = ctx.dependencyResults.find("count_words");
        if (lcIt == ctx.dependencyResults.end() || cwIt == ctx.dependencyResults.end())
            return JobResult::fail("missing dependencies");

        std::string normalized = std::get<std::string>(lcIt->second.value);
        int count = std::get<int>(cwIt->second.value);

        std::ostringstream out;
        out << "NLP Report: " << count << " unique token(s) | Normalized: " << normalized;
        return JobResult::ok(out.str());
    }
};
