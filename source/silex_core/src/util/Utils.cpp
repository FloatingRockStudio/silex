// (C) Copyright 2026 Floating Rock Studio Ltd
// SPDX-License-Identifier: MIT

/// @file Utils.cpp
/// @brief Implementation of general utility functions.

#include "Utils.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <regex>
#include <sstream>

namespace silex {
namespace core {

std::vector<std::string> ensureList(const std::any& value) {
    if (!value.has_value()) {
        return {};
    }

    if (auto* s = std::any_cast<std::string>(&value)) {
        return {*s};
    }
    if (auto* v = std::any_cast<std::vector<std::string>>(&value)) {
        return *v;
    }
    return {};
}

std::any getNestedValue(
    const std::map<std::string, std::any>& m,
    const std::string& keyPath) {

    auto keys = splitString(keyPath, '.');
    const std::map<std::string, std::any>* current = &m;

    for (size_t i = 0; i < keys.size(); ++i) {
        auto it = current->find(keys[i]);
        if (it == current->end()) {
            throw std::out_of_range("Key not found: " + keyPath);
        }

        if (i + 1 < keys.size()) {
            auto* nested = std::any_cast<std::map<std::string, std::any>>(&it->second);
            if (!nested) {
                throw std::out_of_range("Key path is not nested: " + keyPath);
            }
            current = nested;
        } else {
            return it->second;
        }
    }

    return {};
}

std::any getNestedValue(
    const std::map<std::string, std::any>& m,
    const std::string& keyPath,
    const std::any& defaultValue) {
    try {
        return getNestedValue(m, keyPath);
    } catch (const std::out_of_range&) {
        return defaultValue;
    }
}

void setNestedValue(
    std::map<std::string, std::any>& m,
    const std::string& keyPath,
    const std::any& value) {

    auto keys = splitString(keyPath, '.');
    std::map<std::string, std::any>* current = &m;

    for (size_t i = 0; i + 1 < keys.size(); ++i) {
        auto it = current->find(keys[i]);
        if (it == current->end()) {
            (*current)[keys[i]] = std::map<std::string, std::any>{};
            current = std::any_cast<std::map<std::string, std::any>>(
                &(*current)[keys[i]]);
        } else {
            auto* nested = std::any_cast<std::map<std::string, std::any>>(&it->second);
            if (!nested) {
                it->second = std::map<std::string, std::any>{};
                nested = std::any_cast<std::map<std::string, std::any>>(&it->second);
            }
            current = nested;
        }
    }

    (*current)[keys.back()] = value;
}

void appendNestedValue(
    std::map<std::string, std::any>& m,
    const std::string& keyPath,
    const std::any& value,
    bool unique) {

    auto keys = splitString(keyPath, '.');
    std::map<std::string, std::any>* current = &m;

    for (size_t i = 0; i + 1 < keys.size(); ++i) {
        auto it = current->find(keys[i]);
        if (it == current->end()) {
            (*current)[keys[i]] = std::map<std::string, std::any>{};
            current = std::any_cast<std::map<std::string, std::any>>(
                &(*current)[keys[i]]);
        } else {
            current = std::any_cast<std::map<std::string, std::any>>(&it->second);
        }
    }

    auto it = current->find(keys.back());
    if (it == current->end()) {
        (*current)[keys.back()] = std::vector<std::any>{value};
    } else {
        auto* vec = std::any_cast<std::vector<std::any>>(&it->second);
        if (vec) {
            // unique check would need proper comparison; skip for std::any
            vec->push_back(value);
        }
    }
}

std::vector<std::string> splitString(const std::string& str, char delimiter) {
    std::vector<std::string> parts;
    std::istringstream stream(str);
    std::string part;
    while (std::getline(stream, part, delimiter)) {
        parts.push_back(part);
    }
    return parts;
}

std::string joinStrings(const std::vector<std::string>& parts, const std::string& delimiter) {
    std::string result;
    for (size_t i = 0; i < parts.size(); ++i) {
        if (i > 0) result += delimiter;
        result += parts[i];
    }
    return result;
}

std::string toLower(const std::string& str) {
    std::string result = str;
    std::transform(result.begin(), result.end(), result.begin(),
                   [](unsigned char c) { return std::tolower(c); });
    return result;
}

std::string toUpper(const std::string& str) {
    std::string result = str;
    std::transform(result.begin(), result.end(), result.begin(),
                   [](unsigned char c) { return std::toupper(c); });
    return result;
}

std::string toTitle(const std::string& str) {
    std::string result = str;
    bool nextUpper = true;
    for (auto& c : result) {
        if (std::isspace(static_cast<unsigned char>(c)) || c == '_' || c == '-') {
            nextUpper = true;
        } else if (nextUpper) {
            c = std::toupper(static_cast<unsigned char>(c));
            nextUpper = false;
        } else {
            c = std::tolower(static_cast<unsigned char>(c));
        }
    }
    return result;
}

bool globMatch(const std::string& pattern, const std::string& text) {
    const std::string normalizedPattern = toLower(pattern);
    const std::string normalizedText = toLower(text);
    size_t p = 0, t = 0;
    size_t starP = std::string::npos, starT = 0;

    while (t < normalizedText.size()) {
        if (p < normalizedPattern.size() && (normalizedPattern[p] == normalizedText[t] || normalizedPattern[p] == '?')) {
            ++p;
            ++t;
        } else if (p < normalizedPattern.size() && normalizedPattern[p] == '*') {
            starP = p++;
            starT = t;
        } else if (starP != std::string::npos) {
            p = starP + 1;
            t = ++starT;
        } else {
            return false;
        }
    }

    while (p < normalizedPattern.size() && normalizedPattern[p] == '*') {
        ++p;
    }

    return p == normalizedPattern.size();
}

std::string expandEnvironmentVariables(const std::string& input) {
    static const std::regex envPattern(R"(\$\{([^}]+)\})");
    std::string result = input;
    std::smatch match;

    while (std::regex_search(result, match, envPattern)) {
        const char* envValue = std::getenv(match[1].str().c_str());
        std::string replacement = envValue ? envValue : "";
        result = match.prefix().str() + replacement + match.suffix().str();
    }

    return result;
}

void deepMerge(
    std::map<std::string, std::any>& target,
    const std::map<std::string, std::any>& source) {

    for (const auto& [key, srcVal] : source) {
        auto tgtIt = target.find(key);
        if (tgtIt != target.end()) {
            auto* srcMap = std::any_cast<std::map<std::string, std::any>>(&srcVal);
            auto* tgtMap = std::any_cast<std::map<std::string, std::any>>(&tgtIt->second);
            if (srcMap && tgtMap) {
                deepMerge(*tgtMap, *srcMap);
                continue;
            }
        }
        target[key] = srcVal;
    }
}

std::string formatWithVars(
    const std::string& tmpl,
    const std::map<std::string, std::string>& vars) {

    std::string result = tmpl;
    for (const auto& [key, val] : vars) {
        std::string placeholder = "{" + key + "}";
        size_t pos = 0;
        while ((pos = result.find(placeholder, pos)) != std::string::npos) {
            result.replace(pos, placeholder.size(), val);
            pos += val.size();
        }
    }
    return result;
}

} // namespace core
} // namespace silex
