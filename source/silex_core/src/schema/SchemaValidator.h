// (C) Copyright 2026 Floating Rock Studio Ltd
// SPDX-License-Identifier: MIT

#pragma once

/// @file SchemaValidator.h
/// @brief Validates .silex schema files against the JSON schema definition.

#include <nlohmann/json.hpp>

#include <string>

// Forward declare Json::Value
namespace Json {
class Value;
}

namespace silex {
namespace core {

/// Validates .silex schema files against the JSON Schema definition.
class SchemaValidator {
public:
    /// Default constructor.
    SchemaValidator() = default;

    SchemaValidator(const SchemaValidator&) = delete;
    /// Deleted copy assignment operator.
    SchemaValidator& operator=(const SchemaValidator&) = delete;

    /// Load the JSON Schema definition from a file path.
    bool loadSchema(const std::string& schemaPath);

    /// Validate a parsed JSON value against the loaded schema.
    /// Returns true if valid, populates errorMessage on failure.
    bool validate(const Json::Value& document, std::string& errorMessage) const;

    /// Check if a schema has been loaded.
    bool isLoaded() const;

private:
    nlohmann::json m_schemaDoc;
    bool m_loaded = false;
};

} // namespace core
} // namespace silex
