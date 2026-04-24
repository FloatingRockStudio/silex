// (C) Copyright 2026 Floating Rock Studio Ltd
// SPDX-License-Identifier: MIT

#pragma once

/// @file Logging.h
/// @brief Logging infrastructure using spdlog with component-specific loggers.

#include <silex/constants.h>

#include <spdlog/spdlog.h>

#include <memory>
#include <string>

namespace silex {

namespace core {

/// Logger name constants.
namespace LoggerNames {
    inline constexpr const char* Main = "silex";
    inline constexpr const char* SchemaLoader = "silex.schema_loader";
    inline constexpr const char* ExpressionParser = "silex.expression_parser";
    inline constexpr const char* ExpressionEvaluator = "silex.expression_evaluator";
    inline constexpr const char* Resolver = "silex.resolver";
    inline constexpr const char* GenericResolver = "silex.generic_resolver";
    inline constexpr const char* Functor = "silex.functor";
}

/// Get a logger for the specified component.
std::shared_ptr<spdlog::logger> getLogger(const std::string& name);

} // namespace core
} // namespace silex
