// (C) Copyright 2026 Floating Rock Studio Ltd
// SPDX-License-Identifier: MIT

/// @file SchemaUtils.cpp
/// @brief Implementation of schema parsing and segment building utilities.

#include "SchemaUtils.h"
#include "util/Logging.h"
#include "util/Utils.h"

#include <json/value.h>

namespace silex {
namespace core {

Json::Value mergeJsonObjects(const Json::Value& base, const Json::Value& overlay) {
    Json::Value result = base;

    if (!overlay.isObject()) return result;
    if (!result.isObject()) return overlay;

    for (const auto& key : overlay.getMemberNames()) {
        if (result.isMember(key) && result[key].isObject() && overlay[key].isObject()) {
            result[key] = mergeJsonObjects(result[key], overlay[key]);
        } else {
            result[key] = overlay[key];
        }
    }

    return result;
}

std::map<std::string, std::vector<std::string>> extractEndpoints(
    const std::vector<std::shared_ptr<SilexSegment>>& segments,
    const std::string& prefix) {

    std::map<std::string, std::vector<std::string>> endpoints;

    for (const auto& segment : segments) {
        std::string segPath = prefix.empty() ? segment->name : prefix + "." + segment->name;

        if (!segment->endpoint.empty()) {
            endpoints[segment->endpoint].push_back(segPath);
        }

        // Recurse into branches
        for (const auto& [branchKey, branchSegments] : segment->branches) {
            auto branchEndpoints = extractEndpoints(branchSegments, segPath);
            for (auto& [ep, paths] : branchEndpoints) {
                auto& existing = endpoints[ep];
                existing.insert(existing.end(), paths.begin(), paths.end());
            }
        }

        // Recurse into partitions
        if (!segment->partitions.empty()) {
            auto partEndpoints = extractEndpoints(segment->partitions, segPath);
            for (auto& [ep, paths] : partEndpoints) {
                auto& existing = endpoints[ep];
                existing.insert(existing.end(), paths.begin(), paths.end());
            }
        }
    }

    return endpoints;
}

void applyCascadeUpdates(
    std::vector<std::shared_ptr<SilexSegment>>& segments,
    const std::shared_ptr<SilexSegment>& parent) {

    for (auto& segment : segments) {
        if (parent) {
            segment->parent = parent;
        }

        // Recurse into branches
        for (auto& [branchKey, branchSegments] : segment->branches) {
            applyCascadeUpdates(branchSegments, segment);
        }

        // Recurse into partitions
        if (!segment->partitions.empty()) {
            applyCascadeUpdates(segment->partitions, segment);
        }
    }
}

static SegmentFlags parseSegmentFlags(const Json::Value& flagsVal) {
    SegmentFlags flags = SegmentFlags::None;
    if (!flagsVal.isArray()) return flags;

    for (Json::ArrayIndex i = 0; i < flagsVal.size(); ++i) {
        const std::string& flag = flagsVal[i].asString();
        if (flag == "deprecated") flags = flags | SegmentFlags::Deprecated;
        else if (flag == "readonly") flags = flags | SegmentFlags::ReadOnly;
        else if (flag == "promote") flags = flags | SegmentFlags::Promote;
        else if (flag == "omit") flags = flags | SegmentFlags::Omit;
        else if (flag == "intermediate") flags = flags | SegmentFlags::Intermediate;
        else if (flag == "fallback") flags = flags | SegmentFlags::Fallback;
    }

    return flags;
}

static std::map<std::string, Target> parseTargets(const Json::Value& targetsObj) {
    std::map<std::string, Target> targets;
    if (!targetsObj.isObject()) return targets;

    for (const auto& name : targetsObj.getMemberNames()) {
        const auto& tVal = targetsObj[name];
        Target target;

        if (tVal.isString()) {
            target.variable = tVal.asString();
        } else if (tVal.isObject()) {
            if (tVal.isMember("group")) {
                if (tVal["group"].isInt()) {
                    target.group = tVal["group"].asInt();
                } else if (tVal["group"].isString()) {
                    target.group = tVal["group"].asString();
                }
            }
            if (tVal.isMember("variable")) {
                target.variable = tVal["variable"].asString();
            }
            if (tVal.isMember("type")) {
                target.type = tVal["type"].asString();
            }
            if (tVal.isMember("is_array")) {
                target.isArray = tVal["is_array"].asBool();
            }
        }

        targets[name] = target;
    }

    return targets;
}

std::shared_ptr<SilexSegment> parseSegmentFromJson(
    const std::string& name,
    const Json::Value& definition,
    const std::map<std::string, std::map<std::string, Json::Value>>& templates) {

    // If definition has a template reference, expand it by merging template under definition.
    // Template chains are resolved iteratively (A -> B -> C) until no template reference remains.
    Json::Value effectiveDef = definition;
    std::set<std::string> visitedTemplates;
    std::string originalTemplateName;
    while (effectiveDef.isMember("template") && effectiveDef["template"].isString()) {
        std::string tmplName = effectiveDef["template"].asString();
        if (visitedTemplates.empty()) originalTemplateName = tmplName;
        if (visitedTemplates.count(tmplName)) break; // prevent cycles
        visitedTemplates.insert(tmplName);

        auto tmplIt = templates.find(tmplName);
        if (tmplIt == templates.end()) break;

        // Build template JSON from stored map
        Json::Value tmplJson(Json::objectValue);
        for (const auto& [k, v] : tmplIt->second) {
            tmplJson[k] = v;
        }
        // Remove the template key from the overlay before merging so it
        // doesn't overwrite the inner template reference from tmplJson.
        effectiveDef.removeMember("template");
        effectiveDef = mergeJsonObjects(tmplJson, effectiveDef);
    }
    // Remove leftover template key after full chain resolution
    effectiveDef.removeMember("template");
    const Json::Value& def = effectiveDef;

    auto segment = std::make_shared<SilexSegment>();
    segment->name = name;
    segment->templateName = originalTemplateName;

    // Pattern
    if (def.isMember("pattern") && def["pattern"].isString()) {
        segment->pattern = def["pattern"].asString();
    }

    // Generate pattern from options if no explicit pattern is set.
    // "options": ["assets", "shots"] → pattern "(assets|shots)"
    // Case sensitivity is handled by the resolver's config, not embedded here.
    if (!segment->pattern && def.isMember("options") && def["options"].isArray()) {
        const auto& opts = def["options"];
        std::string pat = "(";
        for (Json::ArrayIndex i = 0; i < opts.size(); ++i) {
            if (i > 0) pat += "|";
            pat += opts[i].asString();
        }
        pat += ")";
        segment->pattern = pat;
    }

    // Flags
    if (def.isMember("flags")) {
        segment->flags = parseSegmentFlags(def["flags"]);
    }

    // Deprecated shorthand
    if (def.isMember("deprecated") && def["deprecated"].asBool()) {
        segment->flags = segment->flags | SegmentFlags::Deprecated;
    }

    // is_deprecated shorthand (alternative key)
    if (def.isMember("is_deprecated") && def["is_deprecated"].asBool()) {
        segment->flags = segment->flags | SegmentFlags::Deprecated;
    }

    // is_intermediate shorthand
    if (def.isMember("is_intermediate") && def["is_intermediate"].asBool()) {
        segment->flags = segment->flags | SegmentFlags::Intermediate;
    }

    // is_readonly shorthand
    if (def.isMember("is_readonly") && def["is_readonly"].asBool()) {
        segment->flags = segment->flags | SegmentFlags::ReadOnly;
    }

    // is_fallback shorthand
    if (def.isMember("is_fallback") && def["is_fallback"].asBool()) {
        segment->flags = segment->flags | SegmentFlags::Fallback;
    }

    // Endpoint
    if (def.isMember("endpoint") && def["endpoint"].isString()) {
        segment->endpoint = def["endpoint"].asString();
    }

    // Targets
    bool hasTopLevelTargets = false;
    if (def.isMember("targets")) {
        segment->targets = parseTargets(def["targets"]);
        hasTopLevelTargets = true;
    } else if (def.isMember("target") && def["target"].isString()) {
        // Shorthand: "target": "context.asset" → targets[context.asset] = Target(variable="value")
        Target target;
        target.variable = "value";
        segment->targets[def["target"].asString()] = target;
        hasTopLevelTargets = true;
    }

    // Read expressions
    if (def.isMember("parse")) {
        const auto& readVal = def["parse"];
        if (readVal.isObject()) {
            std::map<std::string, std::any> parseEntry;
            if (readVal.isMember("expressions") && readVal["expressions"].isArray()) {
                std::vector<std::string> exprs;
                for (Json::ArrayIndex i = 0; i < readVal["expressions"].size(); ++i) {
                    exprs.push_back(readVal["expressions"][i].asString());
                }
                parseEntry["expressions"] = std::any(exprs);
            } else if (readVal.isMember("expression") && readVal["expression"].isString()) {
                std::vector<std::string> exprs = {readVal["expression"].asString()};
                parseEntry["expressions"] = std::any(exprs);
            }
            if (readVal.isMember("targets") && readVal["targets"].isObject()) {
                std::map<std::string, std::any> targets;
                for (const auto& tgtKey : readVal["targets"].getMemberNames()) {
                    const auto& tgtVal = readVal["targets"][tgtKey];
                    if (tgtVal.isString()) {
                        targets[tgtKey] = std::any(tgtVal.asString());
                        if (!hasTopLevelTargets && segment->targets.find(tgtKey) == segment->targets.end()) {
                            Target t;
                            t.variable = tgtVal.asString();
                            segment->targets[tgtKey] = t;
                        }
                    } else if (tgtVal.isObject() && tgtVal.isMember("group")) {
                        std::map<std::string, std::any> targetInfo;
                        targetInfo["group"] = std::any(tgtVal["group"].asInt());
                        if (tgtVal.isMember("type") && tgtVal["type"].isString()) {
                            targetInfo["type"] = std::any(tgtVal["type"].asString());
                        }
                        if (tgtVal.isMember("is_array") && tgtVal["is_array"].asBool()) {
                            targetInfo["is_array"] = std::any(true);
                        }
                        targets[tgtKey] = std::any(targetInfo);
                        if (!hasTopLevelTargets && segment->targets.find(tgtKey) == segment->targets.end()) {
                            Target t;
                            t.group = tgtVal["group"].asInt();
                            if (tgtVal.isMember("type") && tgtVal["type"].isString()) {
                                t.type = tgtVal["type"].asString();
                            }
                            if (tgtVal.isMember("is_array") && tgtVal["is_array"].asBool()) {
                                t.isArray = true;
                            }
                            segment->targets[tgtKey] = t;
                        }
                    }
                }
                parseEntry["targets"] = std::any(targets);
            }
            // Handle "target" inside read dict (shorthand for targets)
            if (readVal.isMember("target") && readVal["target"].isString() &&
                !readVal.isMember("targets")) {
                std::string tgtKey = readVal["target"].asString();
                Target target;
                target.variable = "value";
                segment->targets[tgtKey] = target;
            }
            parseEntry["when"] = std::any{};
            parseEntry["parsed"] = std::any{};
            segment->parse.push_back(parseEntry);
        } else if (readVal.isArray()) {
            for (Json::ArrayIndex i = 0; i < readVal.size(); ++i) {
                std::map<std::string, std::any> parseEntry;
                const auto& item = readVal[i];
                if (item.isObject() && item.isMember("expressions")) {
                    std::vector<std::string> exprs;
                    for (Json::ArrayIndex j = 0; j < item["expressions"].size(); ++j) {
                        exprs.push_back(item["expressions"][j].asString());
                    }
                    parseEntry["expressions"] = std::any(exprs);
                } else if (item.isObject() && item.isMember("expression")) {
                    std::vector<std::string> exprs = {item["expression"].asString()};
                    parseEntry["expressions"] = std::any(exprs);
                }
                if (item.isObject() && item.isMember("targets") && item["targets"].isObject()) {
                    std::map<std::string, std::any> targets;
                    for (const auto& tgtKey : item["targets"].getMemberNames()) {
                        const auto& tgtVal = item["targets"][tgtKey];
                        if (tgtVal.isString()) {
                            targets[tgtKey] = std::any(tgtVal.asString());
                        } else if (tgtVal.isObject() && tgtVal.isMember("group")) {
                            std::map<std::string, std::any> targetInfo;
                            targetInfo["group"] = std::any(tgtVal["group"].asInt());
                            if (tgtVal.isMember("type") && tgtVal["type"].isString()) {
                                targetInfo["type"] = std::any(tgtVal["type"].asString());
                            }
                            if (tgtVal.isMember("is_array") && tgtVal["is_array"].asBool()) {
                                targetInfo["is_array"] = std::any(true);
                            }
                            targets[tgtKey] = std::any(targetInfo);
                        }
                    }
                    parseEntry["targets"] = std::any(targets);
                }
                parseEntry["when"] = std::any{};
                parseEntry["parsed"] = std::any{};
                segment->parse.push_back(parseEntry);
            }
        }
    }

    // No auto-default read entry — resolver handles empty read lists

    // Write expressions
    if (def.isMember("format")) {
        const auto& writeVal = def["format"];
        if (writeVal.isString()) {
            std::map<std::string, std::any> formatEntry;
            std::vector<std::string> exprs = {writeVal.asString()};
            formatEntry["expressions"] = std::any(exprs);
            formatEntry["when"] = std::any{};
            formatEntry["parsed"] = std::any{};
            segment->format.push_back(formatEntry);
        } else if (writeVal.isObject()) {
            std::map<std::string, std::any> formatEntry;
            if (writeVal.isMember("expressions") && writeVal["expressions"].isArray()) {
                std::vector<std::string> exprs;
                for (Json::ArrayIndex i = 0; i < writeVal["expressions"].size(); ++i) {
                    exprs.push_back(writeVal["expressions"][i].asString());
                }
                formatEntry["expressions"] = std::any(exprs);
            } else if (writeVal.isMember("expression") && writeVal["expression"].isString()) {
                std::vector<std::string> exprs = {writeVal["expression"].asString()};
                formatEntry["expressions"] = std::any(exprs);
            }
            formatEntry["when"] = std::any{};
            formatEntry["parsed"] = std::any{};
            segment->format.push_back(formatEntry);
        } else if (writeVal.isArray()) {
            // Can be either a list of strings (expressions) or a list of objects (conditional)
            bool allStrings = true;
            for (Json::ArrayIndex i = 0; i < writeVal.size(); ++i) {
                if (!writeVal[i].isString()) { allStrings = false; break; }
            }
            if (allStrings) {
                std::map<std::string, std::any> formatEntry;
                std::vector<std::string> exprs;
                for (Json::ArrayIndex i = 0; i < writeVal.size(); ++i) {
                    exprs.push_back(writeVal[i].asString());
                }
                formatEntry["expressions"] = std::any(exprs);
                formatEntry["when"] = std::any{};
                formatEntry["parsed"] = std::any{};
                segment->format.push_back(formatEntry);
            } else {
                // Conditional write array: [{expression, when}, ...]
                for (Json::ArrayIndex i = 0; i < writeVal.size(); ++i) {
                    const auto& item = writeVal[i];
                    if (!item.isObject()) continue;
                    std::map<std::string, std::any> formatEntry;
                    if (item.isMember("expression") && item["expression"].isString()) {
                        std::vector<std::string> exprs = {item["expression"].asString()};
                        formatEntry["expressions"] = std::any(exprs);
                    }
                    if (item.isMember("when") && item["when"].isObject()) {
                        const auto& when = item["when"];
                        std::map<std::string, std::any> condition;
                        if (when.isMember("key") && when["key"].isString()) {
                            std::vector<std::string> keys = {when["key"].asString()};
                            condition["keys"] = std::any(keys);
                        } else if (when.isMember("keys") && when["keys"].isArray()) {
                            std::vector<std::string> keys;
                            for (Json::ArrayIndex j = 0; j < when["keys"].size(); ++j) {
                                keys.push_back(when["keys"][j].asString());
                            }
                            condition["keys"] = std::any(keys);
                        }
                        if (when.isMember("exists")) {
                            condition["exists"] = std::any(when["exists"].asBool());
                        }
                        formatEntry["when"] = std::any(condition);
                    } else {
                        formatEntry["when"] = std::any{};
                    }
                    formatEntry["parsed"] = std::any{};
                    segment->format.push_back(formatEntry);
                }
            }
        }
    }

    // Auto-generate write expression from pattern+target when no explicit write
    if (segment->format.empty()) {
        std::map<std::string, std::any> formatEntry;
        bool generated = false;
        if (!segment->targets.empty()) {
            // Use the first target key with variable="value" as {targetKey}
            for (const auto& [tgtKey, tgt] : segment->targets) {
                if (tgt.variable && *tgt.variable == "value") {
                    std::vector<std::string> exprs = {"{" + tgtKey + "}"};
                    formatEntry["expressions"] = std::any(exprs);
                    generated = true;
                    break;
                }
            }
        }
        if (!generated && segment->pattern) {
            // Check if pattern is a literal (no regex metacharacters)
            const std::string& pat = *segment->pattern;
            static const std::string metaChars = "()[]{}+*?|^$\\.";
            bool isLiteral = true;
            for (char c : pat) {
                if (metaChars.find(c) != std::string::npos) {
                    isLiteral = false;
                    break;
                }
            }
            if (isLiteral) {
                std::vector<std::string> exprs = {pat};
                formatEntry["expressions"] = std::any(exprs);
                generated = true;
            }
        }
        if (generated) {
            formatEntry["when"] = std::any{};
            formatEntry["parsed"] = std::any{};
            segment->format.push_back(formatEntry);
        }
    }

    // Write update keys
    if (def.isMember("format_update_keys") && def["format_update_keys"].isArray()) {
        for (Json::ArrayIndex i = 0; i < def["format_update_keys"].size(); ++i) {
            segment->formatUpdateKeys.push_back(def["format_update_keys"][i].asString());
        }
    }

    // Placeholders
    if (def.isMember("placeholders") && def["placeholders"].isObject()) {
        const auto& ph = def["placeholders"];
        for (const auto& key : ph.getMemberNames()) {
            if (ph[key].isString()) {
                segment->placeholders[key] = std::any(ph[key].asString());
            }
        }
    }

    // Partition segmenter
    if (def.isMember("partition_segmenter") && def["partition_segmenter"].isString()) {
        segment->partitionSegmenter = def["partition_segmenter"].asString();
    }

    // Ordered partitions
    if (def.isMember("ordered_partitions")) {
        segment->orderedPartitions = def["ordered_partitions"].asBool();
    }

    // Nested paths (branches)
    if (def.isMember("paths") && def["paths"].isObject()) {
        const auto& pathsObj = def["paths"];
        std::vector<std::shared_ptr<SilexSegment>> children;
        for (const auto& childName : pathsObj.getMemberNames()) {
            children.push_back(parseSegmentFromJson(childName, pathsObj[childName], templates));
        }
        if (!children.empty()) {
            segment->branches[""] = children;
        }
    }

    // Switch branches
    if (def.isMember("switch") && def["switch"].isArray()) {
        const auto& switchArr = def["switch"];
        for (Json::ArrayIndex i = 0; i < switchArr.size(); ++i) {
            const auto& switchCase = switchArr[i];
            std::string branchKey = switchCase.isMember("value") ? switchCase["value"].asString() : std::to_string(i);
            if (switchCase.isMember("paths") && switchCase["paths"].isObject()) {
                std::vector<std::shared_ptr<SilexSegment>> branchChildren;
                for (const auto& childName : switchCase["paths"].getMemberNames()) {
                    branchChildren.push_back(
                        parseSegmentFromJson(childName, switchCase["paths"][childName], templates));
                }
                segment->branches[branchKey] = branchChildren;
            }
            // Branch-level endpoint
            if (switchCase.isMember("endpoint") && switchCase["endpoint"].isString()) {
                segment->branchEndpoints[branchKey] = switchCase["endpoint"].asString();
            }
        }
    }

    // Partitions
    if (def.isMember("partitions") && def["partitions"].isObject()) {
        const auto& partObj = def["partitions"];
        for (const auto& partName : partObj.getMemberNames()) {
            segment->partitions.push_back(parseSegmentFromJson(partName, partObj[partName], templates));
        }
        segment->flags = segment->flags | SegmentFlags::HasPartitions;
    } else if (def.isMember("partitions") && def["partitions"].isArray()) {
        const auto& partArr = def["partitions"];
        for (Json::ArrayIndex i = 0; i < partArr.size(); ++i) {
            const auto& partDef = partArr[i];
            std::string partName = partDef.isMember("name") ? partDef["name"].asString() : "partition_" + std::to_string(i);
            segment->partitions.push_back(parseSegmentFromJson(partName, partDef, templates));
        }
        segment->flags = segment->flags | SegmentFlags::HasPartitions;
    }

    return segment;
}

std::vector<std::shared_ptr<SilexSegment>> parseRootSegments(
    const Json::Value& pathsObj,
    const std::map<std::string, std::map<std::string, Json::Value>>& templates) {

    std::vector<std::shared_ptr<SilexSegment>> segments;
    if (!pathsObj.isObject()) return segments;

    for (const auto& name : pathsObj.getMemberNames()) {
        segments.push_back(parseSegmentFromJson(name, pathsObj[name], templates));
    }

    return segments;
}

SilexSchemaInfo parseSchemaInfo(const Json::Value& doc, const std::string& filePath) {
    SilexSchemaInfo info;
    info.path = filePath;
    info.uid = doc.get("uid", "").asString();
    info.rootPath = doc.get("root_path", "").asString();
    info.pathPattern = doc.get("pattern", "").asString();
    info.segmenterUid = doc.get("segmenter", "").asString();

    // Extends / Inherits
    if (doc.isMember("extends") && doc["extends"].isString()) {
        info.extends = doc["extends"].asString();
    } else if (doc.isMember("inherits") && doc["inherits"].isString()) {
        info.extends = doc["inherits"].asString();
    }

    // Functor UIDs
    if (doc.isMember("functors") && doc["functors"].isArray()) {
        for (Json::ArrayIndex i = 0; i < doc["functors"].size(); ++i) {
            const auto& f = doc["functors"][i];
            if (f.isString()) {
                info.functorUids.push_back(f.asString());
            } else if (f.isObject() && f.isMember("uid")) {
                info.functorUids.push_back(f["uid"].asString());
            }
        }
    }

    // Functor aliases
    if (doc.isMember("aliases") && doc["aliases"].isObject()) {
        for (const auto& key : doc["aliases"].getMemberNames()) {
            info.functorAliases[key] = doc["aliases"][key].asString();
        }
    }

    // Context filters
    if (doc.isMember("context_filters") && doc["context_filters"].isArray()) {
        for (Json::ArrayIndex i = 0; i < doc["context_filters"].size(); ++i) {
            std::map<std::string, std::any> filter;
            const auto& filterObj = doc["context_filters"][i];
            for (const auto& key : filterObj.getMemberNames()) {
                filter[key] = std::any(filterObj[key].asString());
            }
            info.contextFilters.push_back(filter);
        }
    }

    return info;
}

SilexConfig parseConfig(const Json::Value& configObj) {
    SilexConfig config;

    if (!configObj.isObject()) return config;

    if (configObj.isMember("case_sensitive")) {
        config.caseSensitive = configObj["case_sensitive"].asBool();
    }

    // Parse variables into ContextMap (recursive for nested objects/arrays)
    std::function<ContextMap(const Json::Value&)> parseVarMap;
    parseVarMap = [&parseVarMap](const Json::Value& obj) -> ContextMap {
        ContextMap result;
        if (!obj.isObject()) return result;
        for (const auto& key : obj.getMemberNames()) {
            const auto& val = obj[key];
            if (val.isString()) {
                result[key] = std::any(val.asString());
            } else if (val.isInt()) {
                result[key] = std::any(val.asInt());
            } else if (val.isBool()) {
                result[key] = std::any(val.asBool());
            } else if (val.isDouble()) {
                result[key] = std::any(val.asDouble());
            } else if (val.isObject()) {
                result[key] = std::any(parseVarMap(val));
            } else if (val.isArray()) {
                std::vector<std::string> arr;
                for (Json::ArrayIndex i = 0; i < val.size(); ++i) {
                    if (val[i].isString()) arr.push_back(val[i].asString());
                }
                result[key] = std::any(arr);
            }
        }
        return result;
    };

    if (configObj.isMember("variables")) {
        config.globalVariables = parseVarMap(configObj["variables"]);
    }
    if (configObj.isMember("functor_variables")) {
        config.functorVariables = parseVarMap(configObj["functor_variables"]);
    }
    if (configObj.isMember("placeholder_variables")) {
        config.placeholderVariables = parseVarMap(configObj["placeholder_variables"]);
    }

    return config;
}

} // namespace core
} // namespace silex
