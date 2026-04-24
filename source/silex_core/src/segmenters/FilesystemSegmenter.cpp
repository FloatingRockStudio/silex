// (C) Copyright 2026 Floating Rock Studio Ltd
// SPDX-License-Identifier: MIT

/// @file FilesystemSegmenter.cpp
/// @brief Implementation of filesystem path segmentation.

#include "FilesystemSegmenter.h"
#include "util/Utils.h"

#include <algorithm>
#include <filesystem>
#include <regex>

namespace fs = std::filesystem;

namespace silex {
namespace segmenters {

std::string FilesystemSegmenter::pathPattern() const {
    // Matches Windows (C:\...) and Unix (/...) paths
    return R"((?:[A-Za-z]:[/\\]|/).*)"  ;
}

std::vector<std::string> FilesystemSegmenter::splitPath(
    const std::string& rootPath, const std::string& path) const {

    // Normalize separators
    std::string normalizedPath = path;
    std::replace(normalizedPath.begin(), normalizedPath.end(), '\\', '/');

    std::string normalizedRoot = rootPath;
    std::replace(normalizedRoot.begin(), normalizedRoot.end(), '\\', '/');

    // Remove trailing slash from root
    while (!normalizedRoot.empty() && normalizedRoot.back() == '/') {
        normalizedRoot.pop_back();
    }

    // Remove root from path
    std::string relative = normalizedPath;
    if (!normalizedRoot.empty() && normalizedPath.find(normalizedRoot) == 0) {
        relative = normalizedPath.substr(normalizedRoot.size());
    }

    // Remove leading slash
    while (!relative.empty() && relative.front() == '/') {
        relative.erase(relative.begin());
    }

    // Split by /
    return core::splitString(relative, '/');
}

std::string FilesystemSegmenter::joinSegments(
    const std::string& rootPath, const std::vector<std::string>& segments) const {

    std::string normalizedRoot = rootPath;
    std::replace(normalizedRoot.begin(), normalizedRoot.end(), '\\', '/');

    // Remove trailing slash
    while (!normalizedRoot.empty() && normalizedRoot.back() == '/') {
        normalizedRoot.pop_back();
    }

    std::string result = normalizedRoot;
    for (const auto& seg : segments) {
        if (!seg.empty()) {
            result += "/" + seg;
        }
    }

#ifdef _WIN32
    // Normalize to OS-native separators on Windows.
    std::replace(result.begin(), result.end(), '/', '\\');
#endif

    return result;
}

bool FilesystemSegmenter::matchesRoot(
    const std::string& rootPath, const std::string& path) const {

    std::string normalizedRoot = rootPath;
    std::replace(normalizedRoot.begin(), normalizedRoot.end(), '\\', '/');
    std::string normalizedPath = path;
    std::replace(normalizedPath.begin(), normalizedPath.end(), '\\', '/');

    // Remove trailing slashes
    while (!normalizedRoot.empty() && normalizedRoot.back() == '/') {
        normalizedRoot.pop_back();
    }

    // Handle wildcards in root path
    if (normalizedRoot.find('*') != std::string::npos ||
        normalizedRoot.find('?') != std::string::npos) {
        return core::globMatch(normalizedRoot, normalizedPath.substr(0, normalizedRoot.size()));
    }

    // Case-insensitive comparison on Windows
#ifdef _WIN32
    std::string lowerRoot = core::toLower(normalizedRoot);
    std::string lowerPath = core::toLower(normalizedPath);
    return lowerPath.find(lowerRoot) == 0;
#else
    return normalizedPath.find(normalizedRoot) == 0;
#endif
}

} // namespace segmenters
} // namespace silex
