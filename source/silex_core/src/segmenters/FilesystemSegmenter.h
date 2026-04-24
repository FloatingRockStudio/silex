// (C) Copyright 2026 Floating Rock Studio Ltd
// SPDX-License-Identifier: MIT

#pragma once

/// @file FilesystemSegmenter.h
/// @brief Segmenter for Windows and Unix filesystem paths.

#include <silex/interfaces/ISegmenter.h>

namespace silex {
namespace segmenters {

/// Path segmenter for filesystem paths (Windows and Linux).
class FilesystemSegmenter : public ISegmenter {
public:
    /// Return the filesystem path separator pattern.
    std::string pathPattern() const override;
    /// Split a filesystem path into individual segments.
    std::vector<std::string> splitPath(
        const std::string& rootPath, const std::string& path) const override;
    /// Join path segments into a filesystem path string.
    std::string joinSegments(
        const std::string& rootPath, const std::vector<std::string>& segments) const override;
    /// Check if a path matches the filesystem root pattern.
    bool matchesRoot(
        const std::string& rootPath, const std::string& path) const override;
};

} // namespace segmenters
} // namespace silex
