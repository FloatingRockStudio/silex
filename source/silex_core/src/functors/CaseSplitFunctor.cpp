/// @file CaseSplitFunctor.cpp
/// @brief Implementation of case splitting functors (camelCase, snake_case).

#include "CaseSplitFunctor.h"
#include "util/Utils.h"
#include "util/Logging.h"
#include <silex/constants.h>

#include <algorithm>
#include <cctype>
#include <regex>
#include <set>
#include <variant>

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

/// Parse output option for explicit index: "0", "1", "-1", etc.
bool tryParseIndex(const std::string& opt, int& index) {
    try {
        index = std::stoi(opt);
        return true;
    } catch (...) {
        return false;
    }
}

/// Parse range specification like "0--2", "1-3", "0--1".
/// Returns true if parsed, with start and end indices set.
bool tryParseRange(const std::string& opt, int& startIdx, int& endIdx) {
    // Pattern: digits then '-' then optional '-' and digits
    // "0--2" → start=0, end=-2; "1-3" → start=1, end=3
    static const std::regex rangePattern(R"(^(-?\d+)-(-?\d+)$)");
    std::smatch match;
    if (std::regex_match(opt, match, rangePattern)) {
        try {
            startIdx = std::stoi(match[1].str());
            endIdx = std::stoi(match[2].str());
            return true;
        } catch (...) {}
    }
    return false;
}

} // anonymous namespace

// MARK: AbstractSplitPartsFunctor

ParseResult AbstractSplitPartsFunctor::parse(
    const std::vector<FunctorInput>& inputs,
    const std::vector<FunctorOutput>& outputs,
    const FunctorContext& context) {

    if (inputs.empty()) {
        return ParseResult{false, "No input value provided", {}};
    }

    if (inputs.size() > 1) {
        return ParseResult{false, "Split functor expects exactly one input value", {}};
    }

    if (outputs.empty()) {
        return ParseResult{false, "No outputs specified", {}};
    }

    std::string value = inputToStr(inputs[0]);
    if (value.empty()) {
        return ParseResult{true, "Success", {}};
    }

    auto parts = splitStringToParts(value);

    if (parts.empty()) {
        return ParseResult{false, "No parts found in: " + value, {}};
    }

    // Normalize parts to lowercase for consistent output
    for (auto& part : parts) {
        part = core::toLower(part);
    }

    // Parse output specs
    struct OutputSpec {
        std::string name;
        bool isOptional = false;
        bool isGreedy = false;
        int explicitIndex = -999;
        int rangeStart = -999, rangeEnd = -999;
        std::string contextValue; // known value from context
    };

    std::vector<OutputSpec> specs;
    for (const auto& output : outputs) {
        OutputSpec spec;
        spec.name = output.name;

        for (const auto& opt : output.options) {
            if (opt == FunctorParams::Optional) {
                spec.isOptional = true;
            } else if (opt == FunctorParams::Greedy) {
                spec.isGreedy = true;
            } else {
                int rs, re;
                if (tryParseRange(opt, rs, re)) {
                    spec.rangeStart = rs;
                    spec.rangeEnd = re;
                } else {
                    int idx;
                    if (tryParseIndex(opt, idx)) {
                        spec.explicitIndex = idx;
                    }
                }
            }
        }

        // Check if context has a known value for this output name
        try {
            auto ctxVal = core::getNestedValue(context.context, spec.name);
            if (auto* s = std::any_cast<std::string>(&ctxVal)) {
                spec.contextValue = core::toLower(*s);
            }
        } catch (...) {}

        specs.push_back(spec);
    }

    std::map<std::string, ResolvedValue> resultOutputs;

    // Allocate parts to outputs
    // Phase 1: Resolve explicit indices and ranges
    // Phase 2: Resolve context-constrained greedy outputs
    // Phase 3: Resolve remaining greedy/sequential outputs

    // Track which parts are "consumed" by explicit assignments
    std::vector<bool> consumed(parts.size(), false);
    // Track allocated part range per output (-1 = not allocated)
    std::vector<int> allocStart(specs.size(), -1);
    std::vector<int> allocEnd(specs.size(), -1);

    // Phase 1: Explicit indices and ranges
    for (size_t i = 0; i < specs.size(); ++i) {
        auto& spec = specs[i];
        if (spec.rangeStart != -999 && spec.rangeEnd != -999) {
            int rs = spec.rangeStart < 0 ? static_cast<int>(parts.size()) + spec.rangeStart : spec.rangeStart;
            int re = spec.rangeEnd < 0 ? static_cast<int>(parts.size()) + spec.rangeEnd : spec.rangeEnd;
            if (rs < 0) rs = 0;
            if (re >= static_cast<int>(parts.size())) re = static_cast<int>(parts.size()) - 1;
            if (rs <= re) {
                allocStart[i] = rs;
                allocEnd[i] = re;
                for (int k = rs; k <= re; ++k) consumed[k] = true;
            } else if (!spec.isOptional) {
                return ParseResult{false, "Invalid index range", {}};
            }
        } else if (spec.explicitIndex != -999) {
            int idx = spec.explicitIndex;
            if (idx < 0) idx = static_cast<int>(parts.size()) + idx;
            if (idx >= 0 && idx < static_cast<int>(parts.size())) {
                allocStart[i] = idx;
                allocEnd[i] = idx;
                consumed[idx] = true;
            } else if (!spec.isOptional) {
                return ParseResult{false, "Invalid index range", {}};
            }
        }
    }

    // Phase 2: Sequential and greedy outputs — allocate from remaining unconsumed parts in order
    // Build list of unconsumed part indices
    std::vector<int> remaining;
    for (int k = 0; k < static_cast<int>(parts.size()); ++k) {
        if (!consumed[k]) remaining.push_back(k);
    }

    // Count how many sequential/greedy outputs need parts
    std::vector<size_t> needsAlloc;
    for (size_t i = 0; i < specs.size(); ++i) {
        if (allocStart[i] == -1 && (specs[i].explicitIndex == -999 && specs[i].rangeStart == -999)) {
            needsAlloc.push_back(i);
        }
    }

    // Phase 2a: Context-constrained greedy outputs — find their match in remaining parts
    std::set<size_t> contextResolved; // Track outputs resolved by context
    for (size_t na = 0; na < needsAlloc.size(); ++na) {
        size_t i = needsAlloc[na];
        auto& spec = specs[i];
        if (!spec.isGreedy || spec.contextValue.empty()) continue;

        // Try to find a contiguous subsequence of remaining parts that, when combined, matches the context value
        for (size_t start = 0; start < remaining.size(); ++start) {
            std::string combined;
            for (size_t end = start; end < remaining.size(); ++end) {
                if (end > start) {
                    std::vector<std::string> subParts;
                    for (size_t k = start; k <= end; ++k) {
                        subParts.push_back(parts[remaining[k]]);
                    }
                    combined = combinePartsToString(subParts);
                } else {
                    combined = parts[remaining[start]];
                }
                if (core::toLower(combined) == spec.contextValue) {
                    allocStart[i] = remaining[start];
                    allocEnd[i] = remaining[end];
                    contextResolved.insert(i);
                    // Remove from remaining
                    remaining.erase(remaining.begin() + start, remaining.begin() + end + 1);
                    needsAlloc.erase(needsAlloc.begin() + na);
                    --na;
                    goto nextContextOutput;
                }
            }
        }
        nextContextOutput:;
    }

    // Phase 2b: Validate context constraints for already-allocated explicit outputs
    for (size_t i = 0; i < specs.size(); ++i) {
        auto& spec = specs[i];
        if (spec.contextValue.empty() || allocStart[i] == -1) continue;
        // Build combined value
        std::vector<std::string> subParts;
        for (int k = allocStart[i]; k <= allocEnd[i]; ++k) {
            subParts.push_back(parts[k]);
        }
        std::string combined = combinePartsToString(subParts);
        if (core::toLower(combined) != spec.contextValue) {
            return ParseResult{false, "Context value for '" + spec.name + "' doesn't match parts", {}};
        }
    }

    // Phase 2c: Allocate remaining parts to sequential/greedy outputs
    // Required greedy: takes as many as possible, reserving 1 for each output after
    // Optional greedy: splits evenly with other greedy outputs
    {
        size_t remIdx = 0;
        for (size_t na = 0; na < needsAlloc.size(); ++na) {
            size_t i = needsAlloc[na];
            auto& spec = specs[i];
            if (allocStart[i] != -1) continue; // already allocated by context

            if (remIdx >= remaining.size()) {
                if (!spec.isOptional) {
                    return ParseResult{false, "Not enough parts for output " + spec.name, {}};
                }
                continue;
            }

            if (spec.isGreedy) {
                size_t available = remaining.size() - remIdx;

                // Count outputs after this one
                size_t outputsAfter = 0;
                size_t greedyLeft = 1; // count self
                for (size_t na2 = na + 1; na2 < needsAlloc.size(); ++na2) {
                    size_t j = needsAlloc[na2];
                    if (allocStart[j] != -1) continue;
                    outputsAfter++;
                    if (specs[j].isGreedy) greedyLeft++;
                }

                size_t take;
                if (!spec.isOptional) {
                    // Required greedy: take as much as possible, reserve 1 for each output after
                    take = available > outputsAfter ? available - outputsAfter : 1;
                } else {
                    // Optional greedy: fair share with other greedy outputs
                    size_t forGreedy = available > (outputsAfter - (greedyLeft - 1))
                                     ? available - (outputsAfter - (greedyLeft - 1))
                                     : available;
                    take = greedyLeft > 0 ? forGreedy / greedyLeft : forGreedy;
                    if (take == 0) take = 1;
                }

                allocStart[i] = remaining[remIdx];
                allocEnd[i] = remaining[remIdx + take - 1];
                remIdx += take;
            } else {
                allocStart[i] = remaining[remIdx];
                allocEnd[i] = remaining[remIdx];
                remIdx++;
            }
        }
    }

    // Phase 3: Build result outputs
    for (size_t i = 0; i < specs.size(); ++i) {
        auto& spec = specs[i];
        if (allocStart[i] == -1) {
            if (spec.isOptional) continue; // skip optional with no allocation
            continue; // already handled by error above
        }
        std::vector<std::string> subParts;
        for (int k = allocStart[i]; k <= allocEnd[i]; ++k) {
            subParts.push_back(parts[k]);
        }
        std::string combined = combinePartsToString(subParts);
        bool ambiguous = (allocEnd[i] - allocStart[i]) > 0 && spec.isGreedy
                         && contextResolved.find(i) == contextResolved.end();
        resultOutputs[spec.name] = ResolvedValue{combined, ambiguous};
    }

    return ParseResult{true, "Success", resultOutputs};
}

FormatResult AbstractSplitPartsFunctor::format(
    const std::vector<FunctorInput>& inputs,
    const FunctorContext& context) {

    if (inputs.empty()) {
        return FormatResult{false, "No input values provided", ""};
    }

    std::vector<std::string> parts;
    for (const auto& input : inputs) {
        std::string s = inputToStr(input);
        // Trim whitespace
        size_t start = s.find_first_not_of(" \t\r\n");
        if (start != std::string::npos) {
            s = s.substr(start, s.find_last_not_of(" \t\r\n") - start + 1);
        } else {
            s.clear();
        }
        if (!s.empty()) {
            parts.push_back(s);
        }
    }

    if (parts.empty()) {
        return FormatResult{false, "Nothing to write", ""};
    }

    return FormatResult{true, "Success", combinePartsToString(parts)};
}

// MARK: CamelCase

std::vector<std::string> SplitCamelCaseFunctor::splitStringToParts(
    const std::string& value) const {

    std::vector<std::string> parts;
    if (value.empty()) return parts;

    std::string current;
    current += value[0];

    for (size_t i = 1; i < value.size(); ++i) {
        char c = value[i];
        char prev = value[i - 1];

        bool isUpper = std::isupper(static_cast<unsigned char>(c));
        bool prevIsLower = std::islower(static_cast<unsigned char>(prev));
        bool nextIsLower = (i + 1 < value.size()) &&
                           std::islower(static_cast<unsigned char>(value[i + 1]));

        // Split on lower->upper transitions, or upper->upper when followed by lower
        if ((prevIsLower && isUpper) ||
            (isUpper && nextIsLower && !current.empty() &&
             std::isupper(static_cast<unsigned char>(current.back())))) {
            if (!current.empty()) {
                parts.push_back(current);
                current.clear();
            }
        }

        current += c;
    }

    if (!current.empty()) {
        parts.push_back(current);
    }

    // If no alphabetic characters found, return empty (not camelCase)
    bool hasAlpha = false;
    for (const auto& part : parts) {
        for (char c : part) {
            if (std::isalpha(static_cast<unsigned char>(c))) {
                hasAlpha = true;
                break;
            }
        }
        if (hasAlpha) break;
    }
    if (!hasAlpha) return {};

    return parts;
}

std::string SplitCamelCaseFunctor::combinePartsToString(
    const std::vector<std::string>& parts) const {

    std::string result;
    for (size_t i = 0; i < parts.size(); ++i) {
        if (i == 0) {
            // First part lowercase
            result += core::toLower(parts[i]);
        } else {
            // Subsequent parts: capitalize first letter
            if (!parts[i].empty()) {
                result += static_cast<char>(std::toupper(static_cast<unsigned char>(parts[i][0])));
                if (parts[i].size() > 1) {
                    result += parts[i].substr(1);
                }
            }
        }
    }
    return result;
}

// MARK: SnakeCase

std::vector<std::string> SplitSnakeCaseFunctor::splitStringToParts(
    const std::string& value) const {

    return core::splitString(value, '_');
}

std::string SplitSnakeCaseFunctor::combinePartsToString(
    const std::vector<std::string>& parts) const {

    return core::joinStrings(parts, "_");
}

} // namespace functors
} // namespace silex
