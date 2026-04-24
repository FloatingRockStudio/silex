// (C) Copyright 2026 Floating Rock Studio Ltd
// SPDX-License-Identifier: MIT

#pragma once

/// @file Utils.h
/// @brief General utility functions (string splitting, nested map access, glob matching).

#include <any>
#include <map>
#include <optional>
#include <string>
#include <vector>

namespace silex {
namespace core {

/// Ensure a value is wrapped in a vector. Strings become single-element vectors.
std::vector<std::string> ensureList(const std::any& value);

/// Get a nested value from a map using dot notation.
/// Returns std::nullopt if path doesn't exist when no default is provided.
std::any getNestedValue(
    const std::map<std::string, std::any>& m,
    const std::string& keyPath);

/// Get a nested value with a default fallback.
std::any getNestedValue(
    const std::map<std::string, std::any>& m,
    const std::string& keyPath,
    const std::any& defaultValue);

/// Set a nested value in a map using dot notation.
void setNestedValue(
    std::map<std::string, std::any>& m,
    const std::string& keyPath,
    const std::any& value);

/// Append a value to a nested list using dot notation.
void appendNestedValue(
    std::map<std::string, std::any>& m,
    const std::string& keyPath,
    const std::any& value,
    bool unique = false);

/// Split a string by delimiter.
std::vector<std::string> splitString(
    const std::string& str, char delimiter);

/// Join strings with a delimiter.
std::string joinStrings(
    const std::vector<std::string>& parts, const std::string& delimiter);

/// Convert a string to lowercase.
std::string toLower(const std::string& str);

/// Convert a string to uppercase.
std::string toUpper(const std::string& str);

/// Convert a string to title case.
std::string toTitle(const std::string& str);

/// Check if a string matches a glob pattern.
bool globMatch(const std::string& pattern, const std::string& text);

/// Expand environment variables in a string (${VAR} syntax).
std::string expandEnvironmentVariables(const std::string& input);

/// Deep merge source map into target. Nested maps are merged recursively.
void deepMerge(
    std::map<std::string, std::any>& target,
    const std::map<std::string, std::any>& source);

/// Replace {key} placeholders in a string with values from a flat map.
std::string formatWithVars(
    const std::string& tmpl,
    const std::map<std::string, std::string>& vars);

} // namespace core
} // namespace silex
