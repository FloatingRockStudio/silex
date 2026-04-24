// (C) Copyright 2026 Floating Rock Studio Ltd
// SPDX-License-Identifier: MIT

#pragma once

/// @file constants.h
/// @brief Constants, enums, and utility functions for the Silex library.

#include <silex/export.h>

#include <cstdint>
#include <regex>
#include <string>

namespace silex {

// MARK: Enums

/// Bitflags for segment properties (deprecated, read-only, intermediate, etc.).
enum class SegmentFlags : uint32_t {
    None = 0,
    Deprecated = 1,
    ReadOnly = 2,
    Promote = 4,
    Omit = 8,
    Intermediate = 16,
    HasPartitions = 32,
    Fallback = 64
};

inline SegmentFlags operator|(SegmentFlags a, SegmentFlags b) {
    return static_cast<SegmentFlags>(static_cast<uint32_t>(a) | static_cast<uint32_t>(b));
}

inline SegmentFlags operator&(SegmentFlags a, SegmentFlags b) {
    return static_cast<SegmentFlags>(static_cast<uint32_t>(a) & static_cast<uint32_t>(b));
}

/// Check if a specific flag is set in the combined flags value.
inline bool hasFlag(SegmentFlags flags, SegmentFlags flag) {
    return (static_cast<uint32_t>(flags) & static_cast<uint32_t>(flag)) != 0;
}

/// Bitflags for resolution outcome status.
enum class ResolverStatus : uint32_t {
    None = 0,
    Success = 1,
    MissingTargets = 2,
    Partial = 4,
    Ambiguous = 8,
    Error = 16
};

inline ResolverStatus operator|(ResolverStatus a, ResolverStatus b) {
    return static_cast<ResolverStatus>(static_cast<uint32_t>(a) | static_cast<uint32_t>(b));
}

inline ResolverStatus operator&(ResolverStatus a, ResolverStatus b) {
    return static_cast<ResolverStatus>(static_cast<uint32_t>(a) & static_cast<uint32_t>(b));
}

/// Check if a specific flag is set in the combined flags value.
inline bool hasFlag(ResolverStatus flags, ResolverStatus flag) {
    return (static_cast<uint32_t>(flags) & static_cast<uint32_t>(flag)) != 0;
}

/// Mode of expression evaluation (parse extracts context, format generates values).
enum class ExpressionMode {
    Parse,
    Format
};

/// Language identifier for external functor registration.
enum class Language {
    Python,
    JavaScript,
    Other
};

// MARK: Functor Parameters

namespace FunctorParams {
    inline constexpr const char* Optional = "?";
    inline constexpr const char* Greedy = "+";
    inline constexpr const char* FirstIndex = "0";
    inline constexpr const char* LastIndex = "-1";
}

// MARK: Verbosity

/// Verbosity tiers for Silex logging output.
enum class Verbosity : int {
    Quiet = 0,
    Info = 1,
    Flow = 2,
    Detail = 3,
    Trace = 4
};

/// Set global verbosity level for all Silex loggers.
SILEX_API void setVerbosity(Verbosity verbosity);
SILEX_API void setVerbosity(int verbosity);

/// Get current global verbosity level.
SILEX_API Verbosity getVerbosity();

// MARK: Expression Patterns

namespace ExpressionPatterns {
    /// Pattern to match functor calls: $functor_name(args) or $functor_name
    inline const std::regex FUNCTOR_CALL(R"(\$([a-zA-Z_][a-zA-Z0-9_.]*)\s*(?:\((.*)\))?)");
    /// Pattern to split arguments respecting nested structures
    inline const std::regex ARGUMENT_SPLIT(R"(,(?![^(){}]*[)}]))");
    /// Pattern to extract variable names from inputs: {variable_name}
    inline const std::regex VARIABLE_EXTRACT(R"(\{([^}]+)\})");
    /// Pattern to match arrow operator in expressions: ->
    inline const std::regex ARROW_OPERATOR(R"(\s*->\s*)");
}

} // namespace silex
