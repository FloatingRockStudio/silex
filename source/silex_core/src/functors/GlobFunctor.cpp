// (C) Copyright 2026 Floating Rock Studio Ltd
// SPDX-License-Identifier: MIT

/// @file GlobFunctor.cpp
/// @brief Implementation of filesystem glob pattern matching functor.

#include "GlobFunctor.h"
#include "util/Logging.h"
#include "util/Utils.h"

#include <filesystem>
#include <unordered_set>
#include <variant>

namespace fs = std::filesystem;

namespace silex {
namespace functors {

namespace {

std::string inputToStr(const FunctorInput& input) {
    return std::visit([](const auto& v) -> std::string {
        using T = std::decay_t<decltype(v)>;
        if constexpr (std::is_same_v<T, std::string>) return v;
        else if constexpr (std::is_same_v<T, int>) return std::to_string(v);
        else if constexpr (std::is_same_v<T, double>) return std::to_string(v);
        else if constexpr (std::is_same_v<T, bool>) return v ? "true" : "false";
        else return "";
    }, input);
}

bool hasExplicitDefault(const FunctorInput& input) {
    return std::visit([](const auto& v) -> bool {
        using T = std::decay_t<decltype(v)>;
        if constexpr (std::is_same_v<T, std::string>) {
            const auto lowered = core::toLower(v);
            return !lowered.empty() && lowered != "0" && lowered != "false";
        } else if constexpr (std::is_same_v<T, int>) {
            return v != 0;
        } else if constexpr (std::is_same_v<T, double>) {
            return v != 0.0;
        } else if constexpr (std::is_same_v<T, bool>) {
            return v;
        } else {
            return false;
        }
    }, input);
}

/// Perform glob matching on filesystem.
std::vector<std::string> globFiles(const std::string& basePath, const std::string& pattern) {
    std::vector<std::string> matches;

    try {
        fs::path base(basePath);
        if (!fs::exists(base) || !fs::is_directory(base)) {
            return matches;
        }

        for (const auto& entry : fs::directory_iterator(base)) {
            std::string name = entry.path().filename().string();
            if (core::globMatch(pattern, name)) {
                matches.push_back(name);
            }
        }
    } catch (const std::exception&) {
        // silently return empty
    }

    std::sort(matches.begin(), matches.end());
    return matches;
}

} // anonymous namespace

std::vector<std::string> GlobFunctor::cachedGlobFiles(
    const std::string& basePath, const std::string& pattern) {
    std::string key = basePath + '\0' + pattern;
    auto it = m_globCache.find(key);
    if (it != m_globCache.end()) return it->second;
    auto result = globFiles(basePath, pattern);
    m_globCache[key] = result;
    return result;
}

ParseResult GlobFunctor::parse(
    const std::vector<FunctorInput>& inputs,
    const std::vector<FunctorOutput>& outputs,
    const FunctorContext& context) {

    if (inputs.empty()) {
        return ParseResult{false, "No glob pattern provided", {}};
    }

    std::string pattern = inputToStr(inputs[0]);
    auto matches = cachedGlobFiles(context.parent, pattern);

    std::map<std::string, ResolvedValue> resultOutputs;

    if (matches.empty()) {
        return ParseResult{false, "No matches found for pattern: " + pattern, {}};
    }

    bool ambiguous = matches.size() > 1;

    for (const auto& output : outputs) {
        resultOutputs[output.name] = ResolvedValue{matches[0], ambiguous};
    }

    return ParseResult{true, "Success", resultOutputs};
}

FormatResult GlobFunctor::format(
    const std::vector<FunctorInput>& inputs,
    const FunctorContext& context) {

    if (inputs.empty()) {
        return FormatResult{false, "No glob pattern provided", ""};
    }

    const bool hasFallback = inputs.size() > 1 && hasExplicitDefault(inputs.back());
    const size_t patternCount = inputs.size() > 1 ? inputs.size() - 1 : 1;

    std::vector<std::string> orderedMatches;
    std::unordered_set<std::string> seenMatches;
    for (size_t index = 0; index < patternCount; ++index) {
        auto matches = cachedGlobFiles(context.parent, inputToStr(inputs[index]));
        for (const auto& match : matches) {
            if (seenMatches.insert(match).second) {
                orderedMatches.push_back(match);
            }
        }
    }

    if (!orderedMatches.empty()) {
        return FormatResult{
            true,
            "Found " + std::to_string(orderedMatches.size()) + " matches",
            orderedMatches[0],
            orderedMatches,
        };
    }

    if (hasFallback) {
        std::string fallback = inputToStr(inputs.back());
        return FormatResult{true, "No glob match, using fallback", fallback};
    }

    return FormatResult{false, "No matches found for provided patterns", ""};
}

} // namespace functors
} // namespace silex
