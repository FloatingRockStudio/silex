// (C) Copyright 2026 Floating Rock Studio Ltd
// SPDX-License-Identifier: MIT

/// @file ResolverHelpers.cpp
/// @brief Implementation of resolver helper functions.

#include "ResolverHelpers.h"
#include "util/Utils.h"

#include <algorithm>

namespace silex {
namespace core {

std::vector<std::string> flattenPlaceholderKeys(
    const std::map<std::string, std::any>& placeholders,
    const std::string& prefix) {

    std::vector<std::string> keys;
    for (const auto& [key, value] : placeholders) {
        std::string fullKey = prefix.empty() ? key : prefix + "." + key;

        auto* nested = std::any_cast<std::map<std::string, std::any>>(&value);
        if (nested) {
            auto childKeys = flattenPlaceholderKeys(*nested, fullKey);
            keys.insert(keys.end(), childKeys.begin(), childKeys.end());
        } else {
            keys.push_back(fullKey);
        }
    }
    return keys;
}

std::vector<std::string> collectSegmentKeyPaths(const SilexSegment& segment) {
    std::vector<std::string> orderedKeys;
    std::set<std::string> seen;

    auto addKey = [&](const std::string& key) {
        if (!key.empty() && seen.find(key) == seen.end()) {
            orderedKeys.push_back(key);
            seen.insert(key);
        }
    };

    for (const auto& key : segment.formatUpdateKeys) {
        addKey(key);
    }
    for (const auto& [key, _] : segment.targets) {
        addKey(key);
    }
    for (const auto& key : flattenPlaceholderKeys(segment.placeholders)) {
        addKey(key);
    }
    addKey(segment.name);

    return orderedKeys;
}

std::tuple<bool, std::any, std::string> convertTargetValue(
    const std::any& targetValue,
    const std::string& targetType,
    const std::string& targetName,
    const std::string& context) {

    if (targetType == "int") {
        if (auto* iv = std::any_cast<int>(&targetValue)) {
            return {true, *iv, ""};
        }
        if (auto* sv = std::any_cast<std::string>(&targetValue)) {
            try {
                return {true, std::stoi(*sv), ""};
            } catch (...) {
                return {false, {}, "Target '" + targetName + "' value not int in " + context};
            }
        }
        return {false, {}, "Target '" + targetName + "' value not int in " + context};
    }

    if (targetType == "float") {
        if (auto* dv = std::any_cast<double>(&targetValue)) {
            return {true, *dv, ""};
        }
        if (auto* sv = std::any_cast<std::string>(&targetValue)) {
            try {
                return {true, std::stod(*sv), ""};
            } catch (...) {
                return {false, {}, "Target '" + targetName + "' value not float in " + context};
            }
        }
        return {false, {}, "Target '" + targetName + "' value not float in " + context};
    }

    if (targetType == "bool") {
        if (auto* sv = std::any_cast<std::string>(&targetValue)) {
            std::string lower = toLower(*sv);
            return {true, (lower == "1" || lower == "true" || lower == "yes" || lower == "on"), ""};
        }
        if (auto* bv = std::any_cast<bool>(&targetValue)) {
            return {true, *bv, ""};
        }
        return {true, false, ""};
    }

    // Default: return as-is
    return {true, targetValue, ""};
}

} // namespace core
} // namespace silex
