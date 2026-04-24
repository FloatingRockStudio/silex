// (C) Copyright 2026 Floating Rock Studio Ltd
// SPDX-License-Identifier: MIT

#pragma once

/// @file URISegmenter.h
/// @brief Segmenters for URI paths and query parameter strings.

#include <silex/interfaces/ISegmenter.h>

namespace silex {
namespace segmenters {

/// Segmenter for URI paths (scheme://path?query).
class URISegmenter : public ISegmenter {
public:
    /// Return the URI path separator pattern.
    std::string pathPattern() const override;
    /// Split a URI path into individual segments.
    std::vector<std::string> splitPath(
        const std::string& rootPath, const std::string& path) const override;
    /// Join segments into a URI path string.
    std::string joinSegments(
        const std::string& rootPath, const std::vector<std::string>& segments) const override;
    /// Check if a path matches the URI scheme root pattern.
    bool matchesRoot(
        const std::string& rootPath, const std::string& path) const override;
};

/// Segmenter for query parameter strings (?key=value&key=value).
class QueryParamsSegmenter : public ISegmenter {
public:
    /// Return the query parameter separator pattern.
    std::string pathPattern() const override;
    /// Split a URI query string into key-value parameter segments.
    std::vector<std::string> splitPath(
        const std::string& rootPath, const std::string& path) const override;
    /// Join key-value segments into a query parameter string.
    std::string joinSegments(
        const std::string& rootPath, const std::vector<std::string>& segments) const override;
    /// Check if a path starts with a query parameter marker.
    bool matchesRoot(
        const std::string& rootPath, const std::string& path) const override;
};

} // namespace segmenters
} // namespace silex
