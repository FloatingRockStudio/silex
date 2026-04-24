#pragma once

/// @file ResolverHelpers.h
/// @brief Helper functions for path resolution (pattern matching, context flattening).

#include <silex/structs.h>

#include <map>
#include <optional>
#include <string>
#include <tuple>
#include <vector>

namespace silex {
namespace core {

/// Collect dotted key paths from nested placeholder definitions.
std::vector<std::string> flattenPlaceholderKeys(
    const std::map<std::string, std::any>& placeholders,
    const std::string& prefix = "");

/// Gather candidate key paths related to a segment from schema metadata.
std::vector<std::string> collectSegmentKeyPaths(const SilexSegment& segment);

/// Convert a target value to the specified type.
/// Returns (success, convertedValue, errorMessage).
std::tuple<bool, std::any, std::string> convertTargetValue(
    const std::any& targetValue,
    const std::string& targetType,
    const std::string& targetName,
    const std::string& context);

/// Trim a list based on limit. Returns (trimmedItems, wasFlipped).
template<typename T>
std::pair<std::vector<T>, bool> trimList(
    const std::vector<T>& items,
    const std::optional<std::variant<int, std::pair<int, int>>>& limit) {

    if (!limit.has_value()) {
        return {items, false};
    }

    bool flip = false;
    std::vector<T> result;

    if (auto* intLimit = std::get_if<int>(&*limit)) {
        if (*intLimit >= 0) {
            size_t n = std::min(static_cast<size_t>(*intLimit), items.size());
            result.assign(items.begin(), items.begin() + n);
        } else {
            size_t n = std::min(static_cast<size_t>(-*intLimit), items.size());
            result.assign(items.end() - n, items.end());
            flip = true;
        }
    } else if (auto* pairLimit = std::get_if<std::pair<int, int>>(&*limit)) {
        int lo = pairLimit->first;
        int hi = pairLimit->second;
        flip = lo < 0;

        // Python-style slicing
        size_t sz = items.size();
        size_t start = lo < 0 ? (sz + lo) : static_cast<size_t>(lo);
        size_t end = hi < 0 ? (sz + hi) : std::min(static_cast<size_t>(hi), sz);
        start = std::min(start, sz);
        end = std::min(end, sz);

        if (start < end) {
            result.assign(items.begin() + start, items.begin() + end);
        }
    }

    return {result, flip};
}

} // namespace core
} // namespace silex
