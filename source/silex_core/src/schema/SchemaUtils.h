// (C) Copyright 2026 Floating Rock Studio Ltd
// SPDX-License-Identifier: MIT

#pragma once

/// @file SchemaUtils.h
/// @brief Utilities for parsing and building schema structures from JSON.

#include <silex/structs.h>

#include <json/value.h>

#include <map>
#include <string>
#include <vector>

namespace silex {
namespace core {

/// Merge two JSON objects, with overlay taking precedence.
Json::Value mergeJsonObjects(const Json::Value& base, const Json::Value& overlay);

/// Extract endpoint paths from a segment tree.
std::map<std::string, std::vector<std::string>> extractEndpoints(
    const std::vector<std::shared_ptr<SilexSegment>>& segments,
    const std::string& prefix = "");

/// Apply cascade updates to segment tree (propagate flags, parent refs, etc.).
void applyCascadeUpdates(
    std::vector<std::shared_ptr<SilexSegment>>& segments,
    const std::shared_ptr<SilexSegment>& parent = nullptr);

/// Parse a segment definition from JSON into a SilexSegment.
std::shared_ptr<SilexSegment> parseSegmentFromJson(
    const std::string& name,
    const Json::Value& definition,
    const std::map<std::string, std::map<std::string, Json::Value>>& templates = {});

/// Parse all root segments from a schema JSON 'paths' object.
std::vector<std::shared_ptr<SilexSegment>> parseRootSegments(
    const Json::Value& pathsObj,
    const std::map<std::string, std::map<std::string, Json::Value>>& templates = {});

/// Parse schema info from a JSON document without fully loading segments.
SilexSchemaInfo parseSchemaInfo(const Json::Value& doc, const std::string& filePath);

/// Parse SilexConfig from a JSON 'config' object.
SilexConfig parseConfig(const Json::Value& configObj);

} // namespace core
} // namespace silex
