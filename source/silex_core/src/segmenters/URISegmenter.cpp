/// @file URISegmenter.cpp
/// @brief Implementation of URI and query parameter segmentation.

#include "URISegmenter.h"
#include "util/Utils.h"

#include <algorithm>
#include <regex>

namespace silex {
namespace segmenters {

// MARK: URISegmenter

std::string URISegmenter::pathPattern() const {
    return R"(([a-zA-Z0-9\-_+]+)://([^?]+)(\?.*)?)";
}

std::vector<std::string> URISegmenter::splitPath(
    const std::string& rootPath, const std::string& path) const {

    static const std::regex uriPattern(R"(([a-zA-Z0-9\-_+]+)://([^?]+)(\?.*)?)");
    std::smatch match;

    std::string pathStr = path;
    if (!std::regex_match(pathStr, match, uriPattern)) {
        return {};
    }

    std::string scheme = match[1].str();
    std::string uriPath = match[2].str();
    std::string query = match[3].matched ? match[3].str() : "";

    // Split the path component
    auto segments = core::splitString(uriPath, '/');

    // Remove empty segments
    segments.erase(
        std::remove_if(segments.begin(), segments.end(),
                       [](const std::string& s) { return s.empty(); }),
        segments.end());

    // If there's a query string, add it as the last segment
    if (!query.empty()) {
        segments.push_back(query);
    }

    return segments;
}

std::string URISegmenter::joinSegments(
    const std::string& rootPath, const std::vector<std::string>& segments) const {

    // Extract scheme and existing path from root path
    static const std::regex uriPattern(R"(([a-zA-Z0-9\-_+]+)://([^?]*))");
    std::smatch match;
    std::string scheme = "file";
    std::string path;

    if (std::regex_search(rootPath, match, uriPattern)) {
        scheme = match[1].str();
        path = match[2].str();
        // Remove trailing slash
        while (!path.empty() && path.back() == '/') path.pop_back();
    }

    std::string query;

    for (const auto& seg : segments) {
        if (!seg.empty() && seg[0] == '?') {
            query = seg;
        } else if (!seg.empty()) {
            if (!path.empty()) path += "/";
            path += seg;
        }
    }

    std::string result = scheme + "://" + path;
    if (!query.empty()) {
        result += query;
    }

    return result;
}

bool URISegmenter::matchesRoot(
    const std::string& rootPath, const std::string& path) const {

    static const std::regex uriPattern(R"(([a-zA-Z0-9\-_+]+)://)");
    std::smatch rootMatch, pathMatch;

    if (!std::regex_search(rootPath, rootMatch, uriPattern) ||
        !std::regex_search(path, pathMatch, uriPattern)) {
        return false;
    }

    // Same scheme
    return core::toLower(rootMatch[1].str()) == core::toLower(pathMatch[1].str());
}

// MARK: QueryParamsSegmenter

std::string QueryParamsSegmenter::pathPattern() const {
    return R"(\?(.+))";
}

std::vector<std::string> QueryParamsSegmenter::splitPath(
    const std::string& rootPath, const std::string& path) const {

    std::string queryStr = path;
    if (!queryStr.empty() && queryStr[0] == '?') {
        queryStr = queryStr.substr(1);
    }

    // Split on &
    auto params = core::splitString(queryStr, '&');
    return params;
}

std::string QueryParamsSegmenter::joinSegments(
    const std::string& rootPath, const std::vector<std::string>& segments) const {

    if (segments.empty()) return "";
    return "?" + core::joinStrings(segments, "&");
}

bool QueryParamsSegmenter::matchesRoot(
    const std::string& rootPath, const std::string& path) const {

    return !path.empty() && path[0] == '?';
}

} // namespace segmenters
} // namespace silex
