// (C) Copyright 2026 Floating Rock Studio Ltd
// SPDX-License-Identifier: MIT

/// @file Logging.cpp
/// @brief Implementation of spdlog-based logging configuration.

#include "Logging.h"

#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>

#include <mutex>
#include <unordered_map>

namespace silex {

namespace {

Verbosity g_globalVerbosity = Verbosity::Quiet;
std::mutex g_mutex;

spdlog::level::level_enum verbosityToLevel(Verbosity v, const std::string& name) {
    // Map verbosity levels per logger, matching Python implementation
    if (name == core::LoggerNames::ExpressionParser ||
        name == core::LoggerNames::ExpressionEvaluator) {
        switch (v) {
            case Verbosity::Quiet: return spdlog::level::off;
            case Verbosity::Info:  return spdlog::level::warn;
            case Verbosity::Flow:  return spdlog::level::info;
            case Verbosity::Detail: return spdlog::level::debug;
            case Verbosity::Trace: return spdlog::level::trace;
        }
    }
    // Default mapping
    switch (v) {
        case Verbosity::Quiet:  return spdlog::level::off;
        case Verbosity::Info:   return spdlog::level::info;
        case Verbosity::Flow:   return spdlog::level::debug;
        case Verbosity::Detail: return spdlog::level::debug;
        case Verbosity::Trace:  return spdlog::level::trace;
    }
    return spdlog::level::off;
}

} // anonymous namespace

void setVerbosity(Verbosity verbosity) {
    std::lock_guard<std::mutex> lock(g_mutex);
    g_globalVerbosity = verbosity;

    // Update all registered loggers
    spdlog::apply_all([verbosity](std::shared_ptr<spdlog::logger> logger) {
        auto level = verbosityToLevel(verbosity, logger->name());
        logger->set_level(level);
    });
}

void setVerbosity(int verbosity) {
    if (verbosity >= 0 && verbosity <= static_cast<int>(Verbosity::Trace)) {
        setVerbosity(static_cast<Verbosity>(verbosity));
    }
}

Verbosity getVerbosity() {
    std::lock_guard<std::mutex> lock(g_mutex);
    return g_globalVerbosity;
}

namespace core {

std::shared_ptr<spdlog::logger> getLogger(const std::string& name) {
    auto logger = spdlog::get(name);
    if (!logger) {
        logger = spdlog::stdout_color_mt(name);
        auto level = verbosityToLevel(g_globalVerbosity, name);
        logger->set_level(level);

        if (g_globalVerbosity >= Verbosity::Trace) {
            logger->set_pattern("%n:%l: %v");
        } else if (g_globalVerbosity >= Verbosity::Flow) {
            logger->set_pattern("%l: %v");
        } else {
            logger->set_pattern("%v");
        }
    }
    return logger;
}

} // namespace core
} // namespace silex
