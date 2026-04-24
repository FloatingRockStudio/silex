// (C) Copyright 2026 Floating Rock Studio Ltd
// SPDX-License-Identifier: MIT

#pragma once

/// @file IResolver.h
/// @brief Interface for bidirectional path/context resolution.

#include <silex/structs.h>

#include <map>
#include <optional>
#include <string>
#include <vector>

namespace silex {

/// Interface for the main Silex resolver.
class IResolver {
public:
    virtual ~IResolver() = default;

    /// Resolve a path to context, optionally filtering by endpoint and schema.
    virtual SilexContextResolve contextFromPath(
        const std::string& path,
        const std::optional<std::vector<std::string>>& endpoint = std::nullopt,
        const std::optional<std::vector<std::string>>& schema = std::nullopt) = 0;

    /// Resolve context to path, optionally filtering by endpoint and schema.
    virtual SilexPathResolve pathFromContext(
        const ContextMap& context,
        const std::optional<std::vector<std::string>>& endpoint = std::nullopt,
        const std::optional<std::vector<std::string>>& schema = std::nullopt) = 0;

    /// List available template names for given schema(s).
    virtual std::vector<std::string> availableTemplates(
        const std::optional<std::vector<std::string>>& schema = std::nullopt) = 0;

    /// Extract context from value using template parse expressions.
    virtual ContextMap parseTemplateValue(
        const std::string& value,
        const std::string& templateName,
        const std::optional<ContextMap>& context = std::nullopt,
        const std::optional<std::vector<std::string>>& schema = std::nullopt) = 0;

    /// Generate value from context using template format expression.
    virtual std::string formatTemplateValue(
        const ContextMap& context,
        const std::string& templateName,
        const std::optional<std::vector<std::string>>& schema = std::nullopt) = 0;

    /// Get schema UID matching the given path.
    virtual std::optional<std::string> schemaFromPath(const std::string& path) = 0;
};

} // namespace silex
