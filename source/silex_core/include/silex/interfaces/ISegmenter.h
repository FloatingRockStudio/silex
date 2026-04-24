#pragma once

/// @file ISegmenter.h
/// @brief Interface for path segmentation strategies (filesystem, URI, query params).

#include <string>
#include <vector>

namespace silex {

/// Interface for path segmenters that handle different path types.
class ISegmenter {
public:
    virtual ~ISegmenter() = default;

    /// Get the regex pattern for matching paths handled by this segmenter.
    virtual std::string pathPattern() const = 0;

    /// Split a path into segments relative to a root path.
    virtual std::vector<std::string> splitPath(
        const std::string& rootPath, const std::string& path) const = 0;

    /// Join segments into a path relative to a root path.
    virtual std::string joinSegments(
        const std::string& rootPath, const std::vector<std::string>& segments) const = 0;

    /// Check if path matches a root filter pattern (supports wildcards and relative paths).
    virtual bool matchesRoot(
        const std::string& rootPath, const std::string& path) const = 0;
};

} // namespace silex
