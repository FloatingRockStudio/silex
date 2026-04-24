// (C) Copyright 2026 Floating Rock Studio Ltd
// SPDX-License-Identifier: MIT

/// @file FileSchemaLoader.cpp
/// @brief Implementation of JSON5 schema file discovery and parsing.

#include "FileSchemaLoader.h"
#include "util/Logging.h"
#include "registry/Registry.h"
#include "SchemaUtils.h"
#include "util/Utils.h"
#include "expression/ExpressionParser.h"

#include <json5cpp.h>
#include <json/value.h>
#include <json/reader.h>

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <functional>
#include <map>
#include <optional>
#include <regex>
#include <set>
#include <sstream>

namespace fs = std::filesystem;

namespace silex {
namespace core {

namespace {

struct OverridePathToken {
    std::string name;
    std::optional<std::string> branch;
    bool singleWildcard = false;
    bool multiWildcard = false;
};

struct SegmentPathToken {
    std::string name;
    std::string branchToChild;
};

struct OverrideTargetMatch {
    std::shared_ptr<SilexSegment> segment;
    std::string fullPath;
    std::vector<SegmentPathToken> pathTokens;
};

std::vector<OverridePathToken> parseOverrideSelector(const std::string& selector) {
    std::vector<OverridePathToken> tokens;
    for (const auto& part : splitString(selector, '.')) {
        OverridePathToken token;
        if (part == "*") {
            token.singleWildcard = true;
        } else if (part == "**") {
            token.multiWildcard = true;
        } else {
            auto eqPos = part.find('=');
            if (eqPos != std::string::npos) {
                token.name = part.substr(0, eqPos);
                token.branch = part.substr(eqPos + 1);
            } else {
                token.name = part;
            }
        }
        tokens.push_back(token);
    }
    return tokens;
}

bool selectorTokenMatches(const OverridePathToken& selector, const SegmentPathToken& pathToken) {
    if (selector.singleWildcard || selector.multiWildcard) {
        return true;
    }
    if (selector.name != pathToken.name) {
        return false;
    }
    if (selector.branch.has_value() && selector.branch.value() != pathToken.branchToChild) {
        return false;
    }
    return true;
}

bool selectorMatchesSuffix(
    const std::vector<OverridePathToken>& selector,
    const std::vector<SegmentPathToken>& path,
    size_t selectorIndex,
    size_t pathIndex) {

    if (selectorIndex == selector.size()) {
        return pathIndex == path.size();
    }

    if (selector[selectorIndex].multiWildcard) {
        for (size_t nextIndex = pathIndex; nextIndex <= path.size(); ++nextIndex) {
            if (selectorMatchesSuffix(selector, path, selectorIndex + 1, nextIndex)) {
                return true;
            }
        }
        return false;
    }

    if (pathIndex >= path.size()) {
        return false;
    }

    if (!selectorTokenMatches(selector[selectorIndex], path[pathIndex])) {
        return false;
    }

    return selectorMatchesSuffix(selector, path, selectorIndex + 1, pathIndex + 1);
}

bool selectorMatchesAnySuffix(
    const std::vector<OverridePathToken>& selector,
    const std::vector<SegmentPathToken>& path) {

    for (size_t startIndex = 0; startIndex < path.size(); ++startIndex) {
        if (selectorMatchesSuffix(selector, path, 0, startIndex)) {
            return true;
        }
    }
    return false;
}

void collectOverrideTargets(
    const std::vector<std::shared_ptr<SilexSegment>>& segments,
    std::vector<SegmentPathToken> currentPath,
    std::vector<OverrideTargetMatch>& out) {

    for (const auto& segment : segments) {
        auto path = currentPath;
        path.push_back({segment->name, ""});

        std::ostringstream joined;
        for (size_t index = 0; index < path.size(); ++index) {
            if (index > 0) {
                joined << '.';
            }
            joined << path[index].name;
        }

        out.push_back({segment, joined.str(), path});

        for (const auto& [branch, branchSegments] : segment->branches) {
            auto branchPath = path;
            if (!branchPath.empty()) {
                branchPath.back().branchToChild = branch;
            }
            collectOverrideTargets(branchSegments, branchPath, out);
        }

        if (!segment->partitions.empty()) {
            collectOverrideTargets(segment->partitions, path, out);
        }
    }
}

std::vector<OverrideTargetMatch> findOverrideTargets(
    const std::vector<std::shared_ptr<SilexSegment>>& rootSegments,
    const std::string& selector) {

    std::vector<OverrideTargetMatch> allTargets;
    collectOverrideTargets(rootSegments, {}, allTargets);

    std::vector<OverrideTargetMatch> exactPathMatches;
    for (const auto& target : allTargets) {
        if (target.fullPath == selector) {
            exactPathMatches.push_back(target);
        }
    }
    if (!exactPathMatches.empty()) {
        return exactPathMatches;
    }

    std::vector<OverrideTargetMatch> endpointMatches;
    for (const auto& target : allTargets) {
        if (target.segment->endpoint == selector) {
            endpointMatches.push_back(target);
        }
    }
    if (!endpointMatches.empty()) {
        return endpointMatches;
    }

    if (selector.find('*') == std::string::npos && selector.find('=') == std::string::npos) {
        return {};
    }

    const auto selectorTokens = parseOverrideSelector(selector);
    std::vector<OverrideTargetMatch> selectorMatches;
    for (const auto& target : allTargets) {
        if (!target.pathTokens.empty() && selectorMatchesAnySuffix(selectorTokens, target.pathTokens)) {
            selectorMatches.push_back(target);
        }
    }

    return selectorMatches;
}

void mergeOverrideScalars(
    const Json::Value& overrideVal,
    const std::shared_ptr<SilexSegment>& target,
    const std::shared_ptr<SilexSegment>& parsedOverride) {

    if (overrideVal.isMember("pattern")) {
        target->pattern = parsedOverride->pattern;
    }
    if (overrideVal.isMember("parse")) {
        target->parse = parsedOverride->parse;
    }
    if (overrideVal.isMember("format")) {
        target->format = parsedOverride->format;
    }
    if (overrideVal.isMember("format_update_keys")) {
        target->formatUpdateKeys = parsedOverride->formatUpdateKeys;
    }
    if (overrideVal.isMember("targets") || overrideVal.isMember("target")) {
        target->targets = parsedOverride->targets;
    }
    if (overrideVal.isMember("endpoint")) {
        target->endpoint = parsedOverride->endpoint;
    }
    if (overrideVal.isMember("partition_segmenter")) {
        target->partitionSegmenter = parsedOverride->partitionSegmenter;
    }
    if (overrideVal.isMember("ordered_partitions")) {
        target->orderedPartitions = parsedOverride->orderedPartitions;
    }
    if (overrideVal.isMember("placeholders")) {
        target->placeholders = parsedOverride->placeholders;
    }
    if (overrideVal.isMember("flags") || overrideVal.isMember("deprecated") || overrideVal.isMember("is_deprecated") ||
        overrideVal.isMember("is_intermediate") || overrideVal.isMember("is_readonly") || overrideVal.isMember("is_fallback")) {
        target->flags = target->flags | parsedOverride->flags;
    }
    if (overrideVal.isMember("pattern") && overrideVal["pattern"].asString() == "(?!)") {
        target->flags = target->flags | SegmentFlags::ReadOnly;
    }
}

void appendOverrideChildren(
    const std::shared_ptr<SilexSegment>& target,
    const std::shared_ptr<SilexSegment>& parsedOverride) {

    for (const auto& [branch, children] : parsedOverride->branches) {
        auto& targetChildren = target->branches[branch];
        targetChildren.insert(targetChildren.end(), children.begin(), children.end());
    }
    target->partitions.insert(
        target->partitions.end(),
        parsedOverride->partitions.begin(),
        parsedOverride->partitions.end());
}

void applyOverrideToTarget(
    const Json::Value& overrideVal,
    const std::shared_ptr<SilexSegment>& target,
    const std::map<std::string, std::map<std::string, Json::Value>>& templates) {

    const std::string cascade = overrideVal.get("cascade", "update").asString();
    auto parsedOverride = parseSegmentFromJson(target->name, overrideVal, templates);

    if (cascade == "replace") {
        auto preservedBranches = target->branches;
        auto preservedPartitions = target->partitions;
        auto preservedName = target->name;
        auto preservedTemplate = target->templateName;
        *target = *parsedOverride;
        target->name = preservedName;
        target->templateName = preservedTemplate;
        target->branches = preservedBranches;
        target->partitions = preservedPartitions;
        return;
    }

    if (cascade == "overwrite") {
        auto preservedName = target->name;
        *target = *parsedOverride;
        target->name = preservedName;
        return;
    }

    mergeOverrideScalars(overrideVal, target, parsedOverride);

    if (cascade == "append") {
        appendOverrideChildren(target, parsedOverride);
    }
}

} // namespace

// MARK: Public

FileSchemaLoader::FileSchemaLoader(Registry& registry)
    : m_registry(registry) {}

FileSchemaLoader::~FileSchemaLoader() = default;

void FileSchemaLoader::preload() {
    if (m_preloaded) return;
    discoverFiles();
    resolveInheritance();
    m_preloaded = true;
}

std::vector<SilexSchemaInfo> FileSchemaLoader::match(
    const std::optional<std::string>& uidPattern,
    const std::optional<std::string>& path,
    const std::optional<ContextMap>& context) const {

    auto logger = getLogger(LoggerNames::SchemaLoader);
    std::vector<SilexSchemaInfo> results;

    for (const auto& [uid, info] : m_uidToInfo) {

        // Filter by UID pattern
        if (uidPattern && !matchUidPattern(uid, *uidPattern)) {
            continue;
        }

        // Filter by path
        if (path) {
            bool pathFiltered = false;
            if (!info.rootPath.empty()) {
                // Check if path starts with root
                auto segmenter = m_registry.getSegmenter(info.segmenterUid);
                if (segmenter && !segmenter->matchesRoot(info.rootPath, *path)) {
                    pathFiltered = true;
                }
            }
            if (!pathFiltered && !info.pathPattern.empty()) {
                try {
                    std::string patStr = info.pathPattern;
                    auto flags = std::regex::ECMAScript;
                    if (patStr.substr(0, 4) == "(?i)") {
                        patStr = patStr.substr(4);
                        flags |= std::regex::icase;
                    }
                    std::regex pathRegex(patStr, flags);
                    if (!std::regex_search(*path, pathRegex)) {
                        pathFiltered = true;
                    }
                } catch (const std::regex_error&) {
                    pathFiltered = true;
                }
            }
            if (pathFiltered) continue;
        }

        // Filter by context — schemas with no context_filters don't match
        if (context) {
            if (info.contextFilters.empty()) continue;
            bool contextMatch = false;
            for (const auto& filter : info.contextFilters) {
                bool filterMatch = true;
                for (const auto& [key, filterVal] : filter) {
                    // Resolve dotted key (e.g. "project.code") in nested context
                    std::string ctxValue;
                    bool found = false;
                    try {
                        auto val = getNestedValue(*context, key);
                        if (auto* s = std::any_cast<std::string>(&val)) {
                            ctxValue = *s;
                            found = true;
                        }
                    } catch (...) {}
                    if (!found) {
                        filterMatch = false;
                        break;
                    }
                    // Match filter value as regex
                    auto* filterStr = std::any_cast<std::string>(&filterVal);
                    if (filterStr) {
                        try {
                            std::regex filterRegex(*filterStr);
                            if (!std::regex_match(ctxValue, filterRegex)) {
                                filterMatch = false;
                                break;
                            }
                        } catch (const std::regex_error&) {
                            if (*filterStr != ctxValue) {
                                filterMatch = false;
                                break;
                            }
                        }
                    }
                }
                if (filterMatch) {
                    contextMatch = true;
                    break;
                }
            }
            if (!contextMatch) continue;
        }

        results.push_back(info);
    }

    // Sort by root_path length descending so more specific schemas are tried first.
    // Tie-break by UID length descending (more specific UID = longer).
    std::sort(results.begin(), results.end(),
        [](const SilexSchemaInfo& a, const SilexSchemaInfo& b) {
            if (a.rootPath.size() != b.rootPath.size())
                return a.rootPath.size() > b.rootPath.size();
            return a.uid.size() > b.uid.size();
        });

    return results;
}

SilexSchemaInfo FileSchemaLoader::loadInfo(const std::string& uid) const {
    auto it = m_uidToInfo.find(uid);
    if (it == m_uidToInfo.end()) {
        throw std::invalid_argument("Schema UID '" + uid + "' not found");
    }
    return it->second;
}

std::shared_ptr<SilexSchema> FileSchemaLoader::loadSchema(const std::string& uid) {
    // Check registry cache first
    auto cached = m_registry.getSchema(uid);
    if (cached) {
        return cached;
    }

    auto schema = std::make_shared<SilexSchema>(buildSchema(uid));
    m_registry.cacheSchema(uid, schema);
    return schema;
}

std::vector<std::string> FileSchemaLoader::availableSchema() const {
    std::vector<std::string> uids;
    uids.reserve(m_uidToInfo.size());
    for (const auto& [uid, _] : m_uidToInfo) {
        uids.push_back(uid);
    }
    return uids;
}

const Json::Value* FileSchemaLoader::getMergedDoc(const std::string& uid) const {
    auto it = m_uidToDoc.find(uid);
    if (it != m_uidToDoc.end()) return &it->second;
    return nullptr;
}

// MARK: Private

void FileSchemaLoader::discoverFiles() {
    auto logger = getLogger(LoggerNames::SchemaLoader);

    const char* searchPathEnv = std::getenv("SILEX_SCHEMA_PATH");
    if (!searchPathEnv) {
        logger->warn("SILEX_SCHEMA_PATH environment variable not set");
        return;
    }

    std::string searchPath(searchPathEnv);
    // Split on platform-specific path separator
#ifdef _WIN32
    char pathSep = ';';
#else
    char pathSep = ':';
#endif
    auto paths = splitString(searchPath, pathSep);

    for (const auto& dir : paths) {
        if (dir.empty()) continue;

        fs::path dirPath(dir);
        if (!fs::exists(dirPath) || !fs::is_directory(dirPath)) {
            logger->debug("Search path directory does not exist: {}", dir);
            continue;
        }

        logger->debug("Scanning directory: {}", dir);

        for (const auto& entry : fs::recursive_directory_iterator(dirPath)) {
            if (!entry.is_regular_file()) continue;
            if (entry.path().extension() != ".silex") continue;

            loadFileInfo(entry.path().string());
        }
    }

    logger->info("Discovered {} schema files", m_uidToPath.size());
}

void FileSchemaLoader::resolveInheritance() {
    auto logger = getLogger(LoggerNames::SchemaLoader);

    for (auto& [uid, info] : m_uidToInfo) {
        if (!info.extends.has_value() || info.extends->empty()) continue;

        auto parentIt = m_uidToInfo.find(*info.extends);
        if (parentIt == m_uidToInfo.end()) {
            logger->warn("Parent schema '{}' not found for '{}'", *info.extends, uid);
            continue;
        }

        bool updated = false;
        if (info.segmenterUid.empty()) {
            info.segmenterUid = parentIt->second.segmenterUid;
            updated = true;
        }
        if (info.rootPath.empty()) {
            info.rootPath = parentIt->second.rootPath;
            updated = true;
        }
        if (info.pathPattern.empty()) {
            info.pathPattern = parentIt->second.pathPattern;
            updated = true;
        }
        if (info.functorUids.empty()) {
            info.functorUids = parentIt->second.functorUids;
            updated = true;
        }
        if (info.functorAliases.empty()) {
            info.functorAliases = parentIt->second.functorAliases;
            updated = true;
        }
        if (info.endpoints.empty()) {
            info.endpoints = parentIt->second.endpoints;
            updated = true;
        }

        if (updated) {
            m_registry.registerSchema(info);
            logger->debug("Resolved inheritance for '{}' from '{}'", uid, *info.extends);
        }
    }
}

void FileSchemaLoader::loadFileInfo(const std::string& filePath) {
    auto logger = getLogger(LoggerNames::SchemaLoader);

    try {
        auto doc = parseJson5File(filePath);
        if (doc.isNull()) return;

        // Process imports first
        processImports(doc, filePath);

        // Register functors and segmenters defined in the file
        registerComponents(doc);

        // If this file defines a schema (has 'uid' and either 'paths' or 'inherits'/'extends')
        if (doc.isMember("uid") && doc["uid"].isString()) {
            std::string uid = doc["uid"].asString();

            // Expand environment variables in root_path and resolve relative paths
            if (doc.isMember("root_path") && doc["root_path"].isString()) {
                std::string rootPath = doc["root_path"].asString();
                rootPath = expandEnvironmentVariables(rootPath);
                // Only resolve filesystem paths, not URI-like paths
                if (rootPath.find("://") == std::string::npos) {
                    fs::path rp(rootPath);
                    // Only resolve genuinely relative paths (no root directory).
                    // Paths like "/" have a root directory and should be kept as-is.
                    if (rp.is_relative() && !rp.has_root_directory()) {
                        rp = fs::path(filePath).parent_path() / rp;
                        try {
                            rp = fs::absolute(rp).lexically_normal();
                        } catch (const std::exception&) {
                            // absolute can fail on some paths
                        }
                    }
                    doc["root_path"] = rp.string();
                } else {
                    doc["root_path"] = rootPath;
                }
            }

            auto info = parseSchemaInfo(doc, filePath);

            // Inherit parent schema properties if this schema extends/inherits
            if (info.extends.has_value() && !info.extends->empty()) {
                auto parentIt = m_uidToInfo.find(*info.extends);
                if (parentIt != m_uidToInfo.end()) {
                    if (info.segmenterUid.empty()) info.segmenterUid = parentIt->second.segmenterUid;
                    if (info.rootPath.empty()) info.rootPath = parentIt->second.rootPath;
                    if (info.pathPattern.empty()) info.pathPattern = parentIt->second.pathPattern;
                }
            }

            m_uidToPath[uid] = filePath;
            m_uidToInfo[uid] = info;
            m_uidToDoc[uid] = doc;

            m_registry.registerSchema(info);
            logger->debug("Registered schema: {} from {}", uid, filePath);
        }
    } catch (const std::exception& e) {
        logger->error("Error loading {}: {}", filePath, e.what());
    }
}

Json::Value FileSchemaLoader::parseJson5File(const std::string& filePath) {
    auto logger = getLogger(LoggerNames::SchemaLoader);

    std::ifstream file(filePath);
    if (!file.is_open()) {
        logger->error("Could not open file: {}", filePath);
        return Json::Value(Json::nullValue);
    }

    Json::Value root;
    std::string err;

    if (!Json5::parse(file, root, &err)) {
        logger->error("JSON5 parse error in {}: {}", filePath, err);
        return Json::Value(Json::nullValue);
    }

    return root;
}

void FileSchemaLoader::processImports(Json::Value& doc, const std::string& filePath) {
    if (!doc.isMember("import") || !doc["import"].isArray()) return;

    auto logger = getLogger(LoggerNames::SchemaLoader);
    fs::path basePath = fs::path(filePath).parent_path();

    const auto& imports = doc["import"];
    for (Json::ArrayIndex i = 0; i < imports.size(); ++i) {
        std::string importPath = imports[i].asString();

        // Resolve relative paths
        fs::path resolved;
        if (importPath.substr(0, 2) == "./") {
            resolved = basePath / (importPath.substr(2) + ".silex");
        } else {
            // Search on SILEX_SCHEMA_PATH
            resolved = findSchemaFile(importPath);
        }

        if (resolved.empty() || !fs::exists(resolved)) {
            logger->warn("Import not found: {} (from {})", importPath, filePath);
            continue;
        }

        std::string resolvedStr = resolved.string();

        // Check if already loaded
        bool alreadyLoaded = false;
        for (const auto& [uid, path] : m_uidToPath) {
            if (path == resolvedStr) {
                alreadyLoaded = true;
                break;
            }
        }

        if (!alreadyLoaded) {
            logger->debug("Importing: {} -> {}", importPath, resolvedStr);
            loadFileInfo(resolvedStr);
        }

        // Merge imported templates into this document
        auto importedDoc = parseJson5File(resolvedStr);
        if (importedDoc.isMember("templates") && importedDoc["templates"].isObject()) {
            if (!doc.isMember("templates") || !doc["templates"].isObject()) {
                doc["templates"] = Json::Value(Json::objectValue);
            }
            const auto& importedTemplates = importedDoc["templates"];
            for (const auto& tmplName : importedTemplates.getMemberNames()) {
                if (!doc["templates"].isMember(tmplName)) {
                    doc["templates"][tmplName] = importedTemplates[tmplName];
                }
            }
        }

        // Also merge imported aliases
        if (importedDoc.isMember("aliases") && importedDoc["aliases"].isObject()) {
            if (!doc.isMember("aliases") || !doc["aliases"].isObject()) {
                doc["aliases"] = Json::Value(Json::objectValue);
            }
            const auto& importedAliases = importedDoc["aliases"];
            for (const auto& aliasName : importedAliases.getMemberNames()) {
                if (!doc["aliases"].isMember(aliasName)) {
                    doc["aliases"][aliasName] = importedAliases[aliasName];
                }
            }
        }

        // Merge imported config (variables, placeholders, etc.)
        if (importedDoc.isMember("config") && importedDoc["config"].isObject()) {
            if (!doc.isMember("config") || !doc["config"].isObject()) {
                doc["config"] = Json::Value(Json::objectValue);
            }
            const auto& importedConfig = importedDoc["config"];
            for (const auto& key : importedConfig.getMemberNames()) {
                if (key == "variables" || key == "functor_variables" || key == "placeholder_variables") {
                    // Merge variable maps: imported values don't override existing
                    if (importedConfig[key].isObject()) {
                        if (!doc["config"].isMember(key) || !doc["config"][key].isObject()) {
                            doc["config"][key] = Json::Value(Json::objectValue);
                        }
                        for (const auto& varName : importedConfig[key].getMemberNames()) {
                            if (!doc["config"][key].isMember(varName)) {
                                doc["config"][key][varName] = importedConfig[key][varName];
                            }
                        }
                    }
                } else if (!doc["config"].isMember(key)) {
                    doc["config"][key] = importedConfig[key];
                }
            }
        }
    }
}

fs::path FileSchemaLoader::findSchemaFile(const std::string& name) {
    const char* searchPathEnv = std::getenv("SILEX_SCHEMA_PATH");
    if (!searchPathEnv) return {};

    std::string searchPath(searchPathEnv);
#ifdef _WIN32
    char pathSep = ';';
#else
    char pathSep = ':';
#endif
    auto paths = splitString(searchPath, pathSep);

    for (const auto& dir : paths) {
        fs::path candidate = fs::path(dir) / (name + ".silex");
        if (fs::exists(candidate)) return candidate;
    }

    return {};
}

void FileSchemaLoader::registerComponents(const Json::Value& doc) {
    auto logger = getLogger(LoggerNames::SchemaLoader);

    // Register functors
    if (doc.isMember("functors") && doc["functors"].isArray()) {
        for (Json::ArrayIndex i = 0; i < doc["functors"].size(); ++i) {
            const auto& f = doc["functors"][i];
            if (!f.isObject()) continue;

            SilexFunctorInfo info;
            info.uid = f.get("uid", "").asString();
            info.name = f.get("name", "").asString();
            info.module = f.get("module", "").asString();
            info.package = f.get("package", "silex").asString();

            if (f.isMember("language") && f["language"].isString()) {
                std::string lang = f["language"].asString();
                if (lang == "python") info.language = Language::Python;
                else if (lang == "javascript") info.language = Language::JavaScript;
                else info.language = Language::Other;
            } else {
                info.language = Language::Other; // C++ default
            }

            if (f.isMember("aliases") && f["aliases"].isArray()) {
                for (Json::ArrayIndex j = 0; j < f["aliases"].size(); ++j) {
                    info.aliases.push_back(f["aliases"][j].asString());
                }
            }

            if (info.uid.empty()) {
                info.uid = info.module + "." + info.name;
            }

            // Register with a factory that returns nullptr for external (Python) functors
            // C++ built-in functors will be registered separately
            m_registry.registerFunctor(info, nullptr);
            logger->debug("Registered functor: {} ({})", info.uid, info.name);
        }
    }

    // Register segmenters
    if (doc.isMember("segmenters") && doc["segmenters"].isArray()) {
        for (Json::ArrayIndex i = 0; i < doc["segmenters"].size(); ++i) {
            const auto& s = doc["segmenters"][i];

            ExternalResource info;
            if (s.isString()) {
                // String format: "module.ClassName"
                std::string fullPath = s.asString();
                auto lastDot = fullPath.rfind('.');
                if (lastDot != std::string::npos) {
                    info.uid = fullPath;
                    info.name = fullPath.substr(lastDot + 1);
                    info.module = fullPath.substr(0, lastDot);
                    auto firstDot = info.module.find('.');
                    info.package = firstDot != std::string::npos
                        ? info.module.substr(0, firstDot) : info.module;
                } else {
                    continue;
                }
            } else if (s.isObject()) {
                info.uid = s.get("uid", "").asString();
                info.name = s.get("name", "").asString();
                info.module = s.get("module", "").asString();
                info.package = s.get("package", "silex").asString();
                if (info.uid.empty()) {
                    info.uid = info.module + "." + info.name;
                }
            } else {
                continue;
            }

            m_registry.registerSegmenter(info, nullptr);
            logger->debug("Registered segmenter: {} ({})", info.uid, info.name);
        }
    }
}

SilexSchema FileSchemaLoader::buildSchema(const std::string& uid) {
    auto logger = getLogger(LoggerNames::SchemaLoader);

    auto docIt = m_uidToDoc.find(uid);
    if (docIt == m_uidToDoc.end()) {
        throw std::invalid_argument("Schema UID '" + uid + "' not found");
    }

    Json::Value doc = docIt->second;

    // Handle inheritance (extends or inherits)
    std::string baseUid;
    if (doc.isMember("extends") && doc["extends"].isString()) {
        baseUid = doc["extends"].asString();
    } else if (doc.isMember("inherits") && doc["inherits"].isString()) {
        baseUid = doc["inherits"].asString();
    }
    if (!baseUid.empty()) {
        auto baseDocIt = m_uidToDoc.find(baseUid);
        if (baseDocIt != m_uidToDoc.end()) {
            doc = mergeJsonObjects(baseDocIt->second, doc);
            logger->debug("Schema {} extends {}", uid, baseUid);
        } else {
            logger->warn("Base schema not found for extension: {}", baseUid);
        }
    }

    // Parse templates
    std::map<std::string, std::map<std::string, Json::Value>> templates;
    if (doc.isMember("templates") && doc["templates"].isObject()) {
        const auto& tmplObj = doc["templates"];
        for (const auto& tmplName : tmplObj.getMemberNames()) {
            std::map<std::string, Json::Value> tmpl;
            const auto& tmplDef = tmplObj[tmplName];
            for (const auto& key : tmplDef.getMemberNames()) {
                tmpl[key] = tmplDef[key];
            }
            templates[tmplName] = tmpl;
        }
    }

    // Parse segments
    std::vector<std::shared_ptr<SilexSegment>> rootSegments;
    if (doc.isMember("paths") && doc["paths"].isObject()) {
        rootSegments = parseRootSegments(doc["paths"], templates);
    }

    // Apply segment overrides from the "overrides" section.
    // Supported selectors:
    // - exact segment paths: "project.production_dir.tree"
    // - exact endpoint names: "context.shot"
    // - suffix/glob selectors with optional branch qualifiers: "tree=shots.*.shot.publish"
    if (doc.isMember("overrides") && doc["overrides"].isObject()) {
        const auto& overrides = doc["overrides"];
        for (const auto& selector : overrides.getMemberNames()) {
            const auto& overrideVal = overrides[selector];
            if (!overrideVal.isObject()) continue;

            auto targets = findOverrideTargets(rootSegments, selector);
            if (targets.empty()) {
                logger->debug("Override target '{}' not found in segment tree", selector);
                continue;
            }

            for (const auto& match : targets) {
                applyOverrideToTarget(overrideVal, match.segment, templates);
                logger->debug("Applied override '{}' to segment '{}'", selector, match.fullPath);
            }
        }
    }

    // Apply cascade updates (parent refs, etc.)
    applyCascadeUpdates(rootSegments);

    // Parse aliases from document
    std::map<std::string, std::string> schemaAliases;
    if (doc.isMember("aliases") && doc["aliases"].isObject()) {
        const auto& aliases = doc["aliases"];
        for (const auto& name : aliases.getMemberNames()) {
            schemaAliases[name] = aliases[name].asString();
        }
    }

    // Pre-parse expressions containing $ in parse/format entries
    ExpressionParser parser(m_registry, schemaAliases);
    std::function<void(const std::vector<std::shared_ptr<SilexSegment>>&)> parseSegmentExpressions;
    parseSegmentExpressions = [&](const std::vector<std::shared_ptr<SilexSegment>>& segments) {
        for (const auto& seg : segments) {
            // Parse format expressions
            for (auto& entry : seg->format) {
                auto exprIt = entry.find("expressions");
                if (exprIt != entry.end()) {
                    if (auto* exprs = std::any_cast<std::vector<std::string>>(&exprIt->second)) {
                        bool hasDollar = false;
                        for (const auto& e : *exprs) {
                            if (e.find('$') != std::string::npos) { hasDollar = true; break; }
                        }
                        if (hasDollar) {
                            try {
                                auto graph = parser.parseExpressions(*exprs);
                                entry["parsed"] = std::any(graph);
                            } catch (...) {}
                        }
                    }
                }
            }
            // Parse parse expressions
            for (auto& entry : seg->parse) {
                auto exprIt = entry.find("expressions");
                if (exprIt != entry.end()) {
                    if (auto* exprs = std::any_cast<std::vector<std::string>>(&exprIt->second)) {
                        bool hasDollar = false;
                        for (const auto& e : *exprs) {
                            if (e.find('$') != std::string::npos) { hasDollar = true; break; }
                        }
                        if (hasDollar) {
                            try {
                                auto graph = parser.parseExpressions(*exprs);
                                entry["parsed"] = std::any(graph);
                            } catch (...) {}
                        }
                    }
                }
            }
            // Recurse into branches
            for (auto& [key, branchSegments] : seg->branches) {
                parseSegmentExpressions(branchSegments);
            }
        }
    };
    parseSegmentExpressions(rootSegments);

    // Parse config
    SilexConfig config;
    if (doc.isMember("config")) {
        config = parseConfig(doc["config"]);
    }

    // Build info
    auto info = parseSchemaInfo(doc, m_uidToPath[uid]);

    // Extract endpoints from segment tree
    auto endpoints = extractEndpoints(rootSegments);
    info.endpoints = endpoints;

    // Update stored info with extracted endpoints
    m_uidToInfo[uid].endpoints = endpoints;

    // Build template map for schema
    std::map<std::string, std::map<std::string, std::any>> templateMap;
    for (const auto& [name, tmpl] : templates) {
        std::map<std::string, std::any> tmplAny;
        for (const auto& [key, val] : tmpl) {
            if (val.isString()) {
                tmplAny[key] = std::any(val.asString());
            } else {
                // Store arrays, objects, etc. as Json::Value
                tmplAny[key] = std::any(val);
            }
        }
        templateMap[name] = tmplAny;
    }

    return SilexSchema{info, rootSegments, config, templateMap};
}

bool FileSchemaLoader::matchUidPattern(const std::string& uid, const std::string& pattern) const {
    // Simple glob matching: * matches within dots
    return globMatch(pattern, uid);
}

} // namespace core
} // namespace silex
