// (C) Copyright 2026 Floating Rock Studio Ltd
// SPDX-License-Identifier: MIT

/// @file GenericResolver_p.cpp
/// @brief Implementation of GenericResolver::Impl and internal helpers.

#include "GenericResolver_p.h"

#include "expression/ExpressionEvaluator.h"
#include "expression/ExpressionParser.h"
#include "schema/FileSchemaLoader.h"
#include "util/Logging.h"
#include "registry/Registry.h"
#include "resolver/ResolverHelpers.h"
#include "schema/SchemaUtils.h"
#include "util/Utils.h"

#include <algorithm>
#include <iomanip>
#include <regex>
#include <sstream>
#include <unordered_map>
#include <variant>

namespace silex {
namespace resolvers {

/// Check if an endpoint query matches a segment endpoint.
/// Supports tail matching: "asset.entity" matches segment endpoint "entity".
static bool endpointMatches(const std::string& query, const std::string& segmentEndpoint) {
    if (query == segmentEndpoint) return true;
    // Only allow single-level suffix match when the segment endpoint is
    // unqualified (no dots).  Qualified endpoints like "context.shot" must
    // match exactly to prevent "sceptre.context.shot" from false-matching
    // a different schema's "context.shot" via suffix overlap.
    if (segmentEndpoint.find('.') != std::string::npos) return false;
    std::string suffix = "." + segmentEndpoint;
    if (query.size() > suffix.size() &&
        query.compare(query.size() - suffix.size(), suffix.size(), suffix) == 0) {
        return true;
    }
    return false;
}

/// Format a context value for template substitution.
static std::optional<std::string> formatTemplateScalar(
    const std::any& value,
    const std::string& formatSpec = "") {

    if (auto* ph = std::any_cast<PlaceholderValue>(&value)) {
        return ph->value();
    }
    if (auto* s = std::any_cast<std::string>(&value)) {
        return *s;
    }
    if (auto* i = std::any_cast<int>(&value)) {
        if (!formatSpec.empty() && formatSpec.back() == 'd') {
            std::string widthSpec = formatSpec.substr(0, formatSpec.size() - 1);
            if (!widthSpec.empty()) {
                std::ostringstream stream;
                stream << std::setw(std::stoi(widthSpec)) << std::setfill('0') << *i;
                return stream.str();
            }
        }
        return std::to_string(*i);
    }
    if (auto* d = std::any_cast<double>(&value)) {
        std::ostringstream stream;
        stream << *d;
        return stream.str();
    }
    if (auto* b = std::any_cast<bool>(&value)) {
        return *b ? "true" : "false";
    }

    return std::nullopt;
}

/// Check whether a value tree contains any PlaceholderValue instances.
static bool containsPlaceholderValue(const std::any& value) {
    if (std::any_cast<PlaceholderValue>(&value) != nullptr) {
        return true;
    }
    if (auto* mapValue = std::any_cast<ContextMap>(&value)) {
        for (const auto& [key, child] : *mapValue) {
            (void)key;
            if (containsPlaceholderValue(child)) {
                return true;
            }
        }
    }
    if (auto* vectorValue = std::any_cast<std::vector<std::any>>(&value)) {
        for (const auto& child : *vectorValue) {
            if (containsPlaceholderValue(child)) {
                return true;
            }
        }
    }
    return false;
}

/// Check whether a context contains any PlaceholderValue instances.
static bool contextContainsPlaceholder(const ContextMap& context) {
    for (const auto& [key, value] : context) {
        (void)key;
        if (containsPlaceholderValue(value)) {
            return true;
        }
    }
    return false;
}

// MARK: Impl

GenericResolver::Impl::Impl(core::Registry& reg, const SilexParseOptions& opts,
                            const std::string& cfgId, const std::string& schema)
    : registry(reg), loader(reg), options(opts), configId(cfgId) {
    if (!schema.empty()) {
        defaultSchema = std::vector<std::string>{schema};
    }
}

void GenericResolver::Impl::ensureInitialized() {
    if (!initialized) {
        loader.preload();
        initialized = true;
    }
}

void GenericResolver::Impl::flattenContext(
    const ContextMap& ctx,
    const std::string& prefix,
    std::map<std::string, std::string>& out) {
    for (const auto& [k, v] : ctx) {
        std::string fullKey = prefix.empty() ? k : prefix + "." + k;
        if (auto* s = std::any_cast<std::string>(&v)) {
            out[fullKey] = *s;
        } else if (auto* nested = std::any_cast<ContextMap>(&v)) {
            flattenContext(*nested, fullKey, out);
        } else if (auto* i = std::any_cast<int>(&v)) {
            out[fullKey] = std::to_string(*i);
        }
    }
}

std::vector<SilexSchemaInfo> GenericResolver::Impl::getMatchingSchemas(
    const std::optional<std::vector<std::string>>& schemaFilter,
    const std::optional<std::string>& path) {

    ensureInitialized();

    if (schemaFilter && !schemaFilter->empty()) {
        std::vector<SilexSchemaInfo> results;
        for (const auto& pattern : *schemaFilter) {
            auto matches = loader.match(pattern, path, std::nullopt);
            results.insert(results.end(), matches.begin(), matches.end());
        }
        return results;
    }

    return loader.match(std::nullopt, path, std::nullopt);
}

SegmentResolve GenericResolver::Impl::resolveSegmentParse(
    const SilexSegment& segment,
    const std::string& segmentValue,
    const std::string& parentPath,
    const ContextMap& currentContext,
    const std::shared_ptr<SilexSchema>& schema) {

    auto logger = core::getLogger(core::LoggerNames::Resolver);

    // First check pattern match
    if (segment.pattern) {
        try {
            // Strip (?i) inline flag (not supported by std::regex ECMAScript)
            std::string patStr = *segment.pattern;
            auto flags = std::regex::ECMAScript;
            if (patStr.substr(0, 4) == "(?i)") {
                patStr = patStr.substr(4);
                flags |= std::regex::icase;
            }
            if (!schema->config.caseSensitive) {
                flags |= std::regex::icase;
            }
            std::regex pat(patStr, flags);
            std::smatch match;
            if (!std::regex_match(segmentValue, match, pat)) {
                return SegmentResolve{ResolverStatus::Error,
                    "Pattern mismatch: " + segmentValue + " vs " + *segment.pattern, {}, {}, {}};
            }

            // Process targets from regex groups
            ContextMap newContext;
            std::set<std::string> resolved;

            for (const auto& [targetName, target] : segment.targets) {
                std::string value;
                if (target.group) {
                    if (auto* intGroup = std::get_if<int>(&*target.group)) {
                        if (*intGroup < static_cast<int>(match.size())) {
                            value = match[*intGroup].str();
                        }
                    } else if (auto* strGroup = std::get_if<std::string>(&*target.group)) {
                        // Named group - not directly supported by std::regex
                        // Fall back to sequential
                    }
                } else if (target.variable && *target.variable == "value") {
                    value = segmentValue;
                }

                if (!value.empty()) {
                    auto [ok, converted, err] = core::convertTargetValue(
                        std::any(value), target.type, targetName, segment.name);
                    if (ok) {
                        core::setNestedValue(newContext, targetName, converted);
                        resolved.insert(targetName);
                    }
                }
            }

            // Process read expressions
            if (!segment.parse.empty()) {
                auto parseResult = evaluateParseExpressions(
                    segment, segmentValue, parentPath, currentContext, schema, &match);
                if (parseResult.success) {
                    for (const auto& [key, rv] : parseResult.outputs) {
                        core::setNestedValue(newContext, key, rv.value);
                        resolved.insert(key);
                    }
                }
            }

            // Process partitions (e.g., query params split by &)
            if (hasFlag(segment.flags, SegmentFlags::HasPartitions) &&
                !segment.partitions.empty() && match.size() > 1) {
                std::string partContent = match[1].str();
                // Split partition content using partition segmenter
                std::vector<std::string> partValues;
                if (segment.partitionSegmenter.has_value()) {
                    auto partSeg = registry.getSegmenter(segment.partitionSegmenter.value());
                    if (partSeg) {
                        partValues = partSeg->splitPath("", partContent);
                    }
                }
                if (partValues.empty()) {
                    partValues = core::splitString(partContent, '&');
                }

                // Helper: substitute {group[N]} references in a string
                auto substituteGroupRefs = [](const std::string& input,
                                              const std::smatch& m) -> std::string {
                    static const std::regex groupRefPat(R"(\{group\[(\d+)\]\})");
                    std::string result = input;
                    std::smatch gm;
                    std::string working = result;
                    std::string output;
                    while (std::regex_search(working, gm, groupRefPat)) {
                        int idx = std::stoi(gm[1].str());
                        std::string replacement =
                            (idx < static_cast<int>(m.size())) ? m[idx].str() : "";
                        output += gm.prefix().str() + replacement;
                        working = gm.suffix().str();
                    }
                    output += working;
                    return output;
                };

                // Helper: try matching a partition value against a single partition
                auto tryPartitionMatch = [&](const std::string& pv,
                                             const SilexSegment& part,
                                             ContextMap& ctx,
                                             std::set<std::string>& res) -> bool {
                    if (!part.pattern) return false;
                    try {
                        std::string pPat = *part.pattern;
                        auto pFlags = std::regex::ECMAScript;
                        if (pPat.substr(0, 4) == "(?i)") {
                            pPat = pPat.substr(4);
                            pFlags |= std::regex::icase;
                        }
                        std::regex partPat(pPat, pFlags);
                        std::smatch pm;
                        if (!std::regex_match(pv, pm, partPat)) return false;

                        // Extract from read targets (raw map entries)
                        std::set<std::string> readResolvedKeys;
                        for (const auto& parseEntry : part.parse) {
                            auto tgtIt = parseEntry.find("targets");
                            if (tgtIt == parseEntry.end()) continue;
                            auto* tgts = std::any_cast<std::map<std::string, std::any>>(
                                &tgtIt->second);
                            if (!tgts) continue;
                            for (const auto& [rawKey, tgtVal] : *tgts) {
                                // Resolve {group[N]} in target key name
                                std::string tgtName = substituteGroupRefs(rawKey, pm);

                                int groupIdx = 0;
                                std::string targetType = "string";
                                bool isArray = false;

                                // Parse target info (may be int or map)
                                if (auto* gi = std::any_cast<int>(&tgtVal)) {
                                    groupIdx = *gi;
                                } else if (auto* info = std::any_cast<
                                               std::map<std::string, std::any>>(&tgtVal)) {
                                    auto gIt = info->find("group");
                                    if (gIt != info->end()) {
                                        if (auto* g = std::any_cast<int>(
                                                &gIt->second)) {
                                            groupIdx = *g;
                                        }
                                    }
                                    auto typeIt = info->find("type");
                                    if (typeIt != info->end()) {
                                        if (auto* t = std::any_cast<std::string>(
                                                &typeIt->second)) {
                                            targetType = *t;
                                        }
                                    }
                                    auto arrIt = info->find("is_array");
                                    if (arrIt != info->end()) {
                                        if (auto* a = std::any_cast<bool>(
                                                &arrIt->second)) {
                                            isArray = *a;
                                        }
                                    }
                                }

                                if (groupIdx < static_cast<int>(pm.size())) {
                                    std::string val = pm[groupIdx].str();
                                    auto [ok, conv, err] = core::convertTargetValue(
                                        std::any(val), targetType, tgtName, part.name);
                                    if (ok) {
                                        if (isArray) {
                                            core::appendNestedValue(ctx, tgtName, conv);
                                        } else {
                                            core::setNestedValue(ctx, tgtName, conv);
                                        }
                                        res.insert(tgtName);
                                        readResolvedKeys.insert(tgtName);
                                    }
                                }
                            }
                        }
                        // Also use top-level targets (Target structs) — skip if already resolved from read
                        for (const auto& [rawKey, tgt] : part.targets) {
                            std::string tgtName = substituteGroupRefs(rawKey, pm);
                            if (readResolvedKeys.count(tgtName)) continue;
                            std::string val;
                            if (tgt.group) {
                                if (auto* ig = std::get_if<int>(&*tgt.group)) {
                                    if (*ig < static_cast<int>(pm.size())) {
                                        val = pm[*ig].str();
                                    }
                                }
                            }
                            if (!val.empty()) {
                                auto [ok, conv, err] = core::convertTargetValue(
                                    std::any(val), tgt.type, tgtName, part.name);
                                if (ok) {
                                    if (tgt.isArray) {
                                        core::appendNestedValue(ctx, tgtName, conv);
                                    } else {
                                        core::setNestedValue(ctx, tgtName, conv);
                                    }
                                    res.insert(tgtName);
                                }
                            }
                        }
                        return true;
                    } catch (...) {}
                    return false;
                };

                for (const auto& pv : partValues) {
                    // First pass: try non-fallback partitions
                    bool matched = false;
                    for (const auto& part : segment.partitions) {
                        if (hasFlag(part->flags, SegmentFlags::Fallback)) continue;
                        if (tryPartitionMatch(pv, *part, newContext, resolved)) {
                            matched = true;
                            break;
                        }
                    }
                    // Second pass: try fallback partitions if no specific match
                    if (!matched) {
                        for (const auto& part : segment.partitions) {
                            if (!hasFlag(part->flags, SegmentFlags::Fallback)) continue;
                            if (tryPartitionMatch(pv, *part, newContext, resolved)) {
                                break;
                            }
                        }
                    }
                }
            }

            return SegmentResolve{ResolverStatus::Success, "Success", newContext, resolved, {}};

        } catch (const std::regex_error& e) {
            return SegmentResolve{ResolverStatus::Error,
                std::string("Regex error: ") + e.what(), {}, {}, {}};
        }
    }

    // No pattern - just use segment value directly
    ContextMap newContext;
    std::set<std::string> resolved;

    if (!segment.parse.empty()) {
        auto parseResult = evaluateParseExpressions(
            segment, segmentValue, parentPath, currentContext, schema);
        if (parseResult.success) {
            for (const auto& [key, rv] : parseResult.outputs) {
                core::setNestedValue(newContext, key, rv.value);
                resolved.insert(key);
            }
        }
    }

    return SegmentResolve{ResolverStatus::Success, "Success", newContext, resolved, {}};
}

ParseResult GenericResolver::Impl::evaluateParseExpressions(
    const SilexSegment& segment,
    const std::string& value,
    const std::string& parentPath,
    const ContextMap& context,
    const std::shared_ptr<SilexSchema>& schema,
    const std::smatch* matchGroups) {

    for (const auto& parseEntry : segment.parse) {
        auto exprIt = parseEntry.find("expressions");

        // Handle read entries with targets but no expressions
        // (simple target mapping from regex groups or segment value)
        if (exprIt == parseEntry.end()) {
            auto tgtIt = parseEntry.find("targets");
            if (tgtIt == parseEntry.end()) continue;
            auto* targets = std::any_cast<std::map<std::string, std::any>>(&tgtIt->second);
            if (!targets) continue;

            ParseResult mapped;
            mapped.success = true;
            for (const auto& [contextKey, tgtVal] : *targets) {
                ResolvedValue rv;
                if (auto* outputName = std::any_cast<std::string>(&tgtVal)) {
                    if (*outputName == "value") {
                        rv.value = std::any(value);
                        mapped.outputs[contextKey] = rv;
                    }
                } else if (auto* groupIdx = std::any_cast<int>(&tgtVal)) {
                    if (matchGroups && *groupIdx < static_cast<int>(matchGroups->size())) {
                        rv.value = std::any((*matchGroups)[*groupIdx].str());
                        mapped.outputs[contextKey] = rv;
                    }
                } else if (auto* info = std::any_cast<std::map<std::string, std::any>>(&tgtVal)) {
                    auto gIt = info->find("group");
                    if (gIt != info->end()) {
                        if (auto* gi = std::any_cast<int>(&gIt->second)) {
                            if (matchGroups && *gi < static_cast<int>(matchGroups->size())) {
                                rv.value = std::any((*matchGroups)[*gi].str());
                                mapped.outputs[contextKey] = rv;
                            }
                        }
                    }
                }
            }
            if (!mapped.outputs.empty()) return mapped;
            continue;
        }

        auto* exprs = std::any_cast<std::vector<std::string>>(&exprIt->second);
        if (!exprs || exprs->empty()) continue;

        // Build cache key
        std::string readCacheKey;
        for (const auto& e : *exprs) {
            if (!readCacheKey.empty()) readCacheKey += '\0';
            readCacheKey += e;
        }
        for (const auto& [ak, av] : schema->info.functorAliases) {
            readCacheKey += '\0'; readCacheKey += ak; readCacheKey += '='; readCacheKey += av;
        }
        auto readCacheIt = expressionCache.find(readCacheKey);
        SilexExpressionGraph* readGraphPtr;
        if (readCacheIt != expressionCache.end()) {
            readGraphPtr = &readCacheIt->second;
        } else {
            core::ExpressionParser parser(registry, schema->info.functorAliases);
            auto [inserted, _] = expressionCache.emplace(readCacheKey, parser.parseExpressions(*exprs));
            readGraphPtr = &inserted->second;
        }
        auto& graph = *readGraphPtr;

        FunctorContext functorCtx;
        functorCtx.context = context;
        functorCtx.parent = parentPath;
        functorCtx.segment = segment.name;
        // Inject {value} into context for the expression
        functorCtx.context["value"] = std::any(value);

        // Inject regex match groups as group[0], group[1], etc.
        if (matchGroups) {
            for (size_t i = 0; i < matchGroups->size(); ++i) {
                std::string key = "group[" + std::to_string(i) + "]";
                functorCtx.context[key] = std::any((*matchGroups)[i].str());
            }
        }

        core::ExpressionEvaluator evaluator(registry);
        auto result = evaluator.evaluateGraph(graph, functorCtx, schema->config);

        if (result.success) {
            // Inject "value" as a default output so targets can reference the
            // raw segment value, matching the convention used by the
            // targets-only code path.
            if (result.outputs.find("value") == result.outputs.end()) {
                ResolvedValue rv;
                rv.value = std::any(value);
                result.outputs["value"] = rv;
            }
            // Also inject regex match groups
            if (matchGroups) {
                for (size_t i = 0; i < matchGroups->size(); ++i) {
                    std::string groupKey = std::to_string(i);
                    if (result.outputs.find(groupKey) == result.outputs.end()) {
                        ResolvedValue rv;
                        rv.value = std::any((*matchGroups)[i].str());
                        result.outputs[groupKey] = rv;
                    }
                }
            }

            // Apply read targets to map expression outputs to context keys
            auto tgtIt = parseEntry.find("targets");
            if (tgtIt != parseEntry.end()) {
                auto* targets = std::any_cast<std::map<std::string, std::any>>(&tgtIt->second);
                if (targets) {
                    ParseResult mapped;
                    mapped.success = true;
                    for (const auto& [contextKey, tgtVal] : *targets) {
                        // String value: map from expression output name
                        if (auto* outputName = std::any_cast<std::string>(&tgtVal)) {
                            auto outIt = result.outputs.find(*outputName);
                            if (outIt != result.outputs.end()) {
                                mapped.outputs[contextKey] = outIt->second;
                            }
                        }
                        // Int value: map from regex group
                        else if (auto* groupIdx = std::any_cast<int>(&tgtVal)) {
                            if (matchGroups && *groupIdx < static_cast<int>(matchGroups->size())) {
                                ResolvedValue rv;
                                rv.value = std::any((*matchGroups)[*groupIdx].str());
                                mapped.outputs[contextKey] = rv;
                            }
                        }
                        // Map value: extract group index from stored metadata
                        else if (auto* info = std::any_cast<std::map<std::string, std::any>>(&tgtVal)) {
                            auto gIt = info->find("group");
                            if (gIt != info->end()) {
                                if (auto* gi = std::any_cast<int>(&gIt->second)) {
                                    if (matchGroups && *gi < static_cast<int>(matchGroups->size())) {
                                        ResolvedValue rv;
                                        rv.value = std::any((*matchGroups)[*gi].str());
                                        mapped.outputs[contextKey] = rv;
                                    }
                                }
                            }
                        }
                    }
                    return mapped;
                }
            }
            return result;
        }
    }

    return ParseResult{false, "No parse expressions matched", {}};
}

void GenericResolver::Impl::prepopulateContext(
    const std::vector<std::shared_ptr<SilexSegment>>& segments,
    ContextMap& context,
    const std::shared_ptr<SilexSchema>& schema) {

    auto logger = core::getLogger(core::LoggerNames::Resolver);

    for (const auto& seg : segments) {
        logger->trace("prepopulate visiting segment '{}' formatUpdateKeys={}",
            seg->name, seg->formatUpdateKeys.size());
        if (!seg->formatUpdateKeys.empty()) {
            // Collect candidate key paths
            std::vector<std::string> candidates;
            for (const auto& k : seg->formatUpdateKeys) candidates.push_back(k);
            for (const auto& [k, _] : seg->targets) {
                if (std::find(candidates.begin(), candidates.end(), k) == candidates.end())
                    candidates.push_back(k);
            }

            // Find a non-null value for any candidate key
            std::string segmentValue;
            std::optional<PlaceholderValue> placeholderValue;
            for (const auto& key : candidates) {
                try {
                    auto val = core::getNestedValue(context, key);
                    if (auto* s = std::any_cast<std::string>(&val)) {
                        if (!s->empty()) {
                            segmentValue = *s;
                            break;
                        }
                    }
                    if (auto* ph = std::any_cast<PlaceholderValue>(&val)) {
                        segmentValue = ph->value();
                        placeholderValue = *ph;
                        break;
                    }
                } catch (...) {}
            }

            if (segmentValue.empty()) continue;

            // Check which keys are missing
            bool hasMissing = false;
            for (const auto& k : seg->formatUpdateKeys) {
                try {
                    core::getNestedValue(context, k);
                } catch (...) {
                    hasMissing = true;
                    break;
                }
            }
            if (!hasMissing) continue;

            if (placeholderValue.has_value()) {
                for (const auto& k : seg->formatUpdateKeys) {
                    try {
                        core::getNestedValue(context, k);
                    } catch (...) {
                        core::setNestedValue(context, k, std::any(*placeholderValue));
                    }
                }
                continue;
            }

            // Run read expression on the candidate value
            logger->debug("Prepopulating context for '{}' with value '{}'",
                seg->name, segmentValue);
            auto parseResult = resolveSegmentParse(
                *seg, segmentValue, schema->info.rootPath, context, schema);

            logger->debug("Prepopulate read result for '{}': success={}, outputs={}",
                seg->name,
                hasFlag(parseResult.status, ResolverStatus::Success),
                parseResult.context.size());

            if (parseResult.status == ResolverStatus::Success ||
                hasFlag(parseResult.status, ResolverStatus::Success)) {
                // Deep merge derived keys into context
                core::deepMerge(context, parseResult.context);
                logger->debug("Prepopulate enriched context with {} top-level keys",
                    parseResult.context.size());
            }
        }

        // Recurse into all branches
        for (const auto& [branchKey, branchSegs] : seg->branches) {
            prepopulateContext(branchSegs, context, schema);
        }
    }
}

FormatResult GenericResolver::Impl::evaluateFormatExpressions(
    const SilexSegment& segment,
    const ContextMap& context,
    const std::string& parentPath,
    const std::shared_ptr<SilexSchema>& schema) {

    auto logger = core::getLogger(core::LoggerNames::Resolver);

    // If any referenced context value is a PlaceholderValue, use it directly
    // and skip expression/functor evaluation (used for glob-pattern generation).
    for (const auto& formatEntry : segment.format) {
        auto exprIt = formatEntry.find("expressions");
        if (exprIt == formatEntry.end()) continue;
        auto* exprs = std::any_cast<std::vector<std::string>>(&exprIt->second);
        if (!exprs || exprs->empty()) continue;
        // Scan for {key} references in the expression
        static const std::regex varPattern(R"(\{([^}:]+)(?::([^}]+))?\})");
        for (const auto& expr : *exprs) {
            bool hasPlaceholder = false;
            std::string::const_iterator searchStart = expr.cbegin();
            std::smatch vm;
            while (std::regex_search(searchStart, expr.cend(), vm, varPattern)) {
                std::string key = vm[1].str();
                try {
                    auto val = core::getNestedValue(context, key);
                    if (std::any_cast<PlaceholderValue>(&val)) {
                        hasPlaceholder = true;
                        break;
                    }
                } catch (...) {}
                searchStart = vm.suffix().first;
            }
            if (hasPlaceholder) {
                // For functor expressions (e.g. $glob(...)), skip string
                // substitution entirely and return the placeholder value
                // directly — functors would produce nonsensical output.
                if (!expr.empty() && expr[0] == '$') {
                    for (auto searchIt = expr.cbegin();
                         std::regex_search(searchIt, expr.cend(), vm, varPattern);
                         searchIt = vm.suffix().first) {
                        std::string key = vm[1].str();
                        try {
                            auto val = core::getNestedValue(context, key);
                            if (auto* ph = std::any_cast<PlaceholderValue>(&val)) {
                                logger->debug("Write placeholder (functor shortcut) for '{}': '{}'",
                                    segment.name, ph->value());
                                return FormatResult{true, "Success", ph->value()};
                            }
                        } catch (...) {}
                    }
                }

                // Simple expression: substitute all variables in-place
                std::string result;
                std::string remaining = expr;
                std::smatch m;
                bool allResolved = true;
                while (std::regex_search(remaining, m, varPattern)) {
                    result += m.prefix().str();
                    std::string key = m[1].str();
                    try {
                        auto val = core::getNestedValue(context, key);
                        if (auto* ph = std::any_cast<PlaceholderValue>(&val)) {
                            result += ph->value();
                        } else if (auto* s = std::any_cast<std::string>(&val)) {
                            result += *s;
                        } else if (auto* i = std::any_cast<int>(&val)) {
                            result += std::to_string(*i);
                        } else {
                            allResolved = false;
                            break;
                        }
                    } catch (...) {
                        allResolved = false;
                        break;
                    }
                    remaining = m.suffix().str();
                }
                if (allResolved) {
                    result += remaining;
                    logger->debug("Write placeholder substitution for '{}': '{}'",
                        segment.name, result);
                    return FormatResult{true, "Success", result};
                }
            }
        }
    }

    for (const auto& formatEntry : segment.format) {
        // Check "when" condition if present
        auto whenIt = formatEntry.find("when");
        if (whenIt != formatEntry.end() && whenIt->second.has_value()) {
            auto* condition = std::any_cast<std::map<std::string, std::any>>(&whenIt->second);
            if (condition) {
                auto keysIt = condition->find("keys");
                bool expectExists = true;
                auto existsIt = condition->find("exists");
                if (existsIt != condition->end()) {
                    if (auto* b = std::any_cast<bool>(&existsIt->second)) {
                        expectExists = *b;
                    }
                }
                if (keysIt != condition->end()) {
                    auto* keys = std::any_cast<std::vector<std::string>>(&keysIt->second);
                    if (keys) {
                        bool conditionMet = true;
                        for (const auto& key : *keys) {
                            bool keyExists = false;
                            try {
                                core::getNestedValue(context, key);
                                keyExists = true;
                            } catch (...) {}
                            if (keyExists != expectExists) {
                                conditionMet = false;
                                break;
                            }
                        }
                        if (!conditionMet) {
                            logger->debug("Write 'when' condition not met for '{}'", segment.name);
                            continue;
                        }
                    }
                }
            }
        }

        auto exprIt = formatEntry.find("expressions");
        if (exprIt == formatEntry.end()) continue;

        auto* exprs = std::any_cast<std::vector<std::string>>(&exprIt->second);
        if (!exprs || exprs->empty()) continue;

        logger->debug("Write expr for '{}': '{}'", segment.name, (*exprs)[0]);

        // Build cache key from all expression strings
        std::string cacheKey;
        for (const auto& e : *exprs) {
            if (!cacheKey.empty()) cacheKey += '\0';
            cacheKey += e;
        }
        // Add schema aliases to cache key (they affect parsing)
        for (const auto& [ak, av] : schema->info.functorAliases) {
            cacheKey += '\0';
            cacheKey += ak;
            cacheKey += '=';
            cacheKey += av;
        }

        auto cacheIt = expressionCache.find(cacheKey);
        SilexExpressionGraph* graphPtr;
        if (cacheIt != expressionCache.end()) {
            graphPtr = &cacheIt->second;
        } else {
            core::ExpressionParser parser(registry, schema->info.functorAliases);
            auto [inserted, _] = expressionCache.emplace(cacheKey, parser.parseExpressions(*exprs));
            graphPtr = &inserted->second;
        }
        auto& graph = *graphPtr;

        if (graph.expressions.empty()) {
            logger->debug("Write parse produced 0 expressions for '{}'", segment.name);
            continue;
        }

        FunctorContext functorCtx;
        functorCtx.context = context;
        functorCtx.parent = parentPath;
        functorCtx.segment = segment.name;

        // For write, evaluate the last expression
        auto& lastExpr = graph.expressions.back();

        // Handle simple template expressions (no functor, e.g., "{context.asset}")
        if (!lastExpr.functorInfo && lastExpr.inputs.empty()) {
            std::string raw = lastExpr.raw;
            static const std::regex varPattern(R"(\{([^}:]+)(?::([^}]+))?\})");
            std::string resolved = raw;
            std::smatch vm;
            bool allResolved = true;
            bool hasVariables = false;
            while (std::regex_search(resolved, vm, varPattern)) {
                hasVariables = true;
                std::string key = vm[1].str();
                std::string formatSpec = vm[2].matched ? vm[2].str() : "";
                try {
                    auto val = core::getNestedValue(context, key);
                    auto formatted = formatTemplateScalar(val, formatSpec);
                    if (!formatted.has_value()) {
                        allResolved = false;
                        break;
                    }
                    resolved = vm.prefix().str() + *formatted + vm.suffix().str();
                } catch (...) {
                    allResolved = false;
                    break;
                }
            }
            if (!hasVariables) {
                logger->debug("Write result for '{}': success=true output='{}'",
                    segment.name, raw);
                return FormatResult{true, "Success", raw};
            }
            if (allResolved) {
                logger->debug("Write result for '{}': success=true output='{}'",
                    segment.name, resolved);
                return FormatResult{true, "Success", resolved};
            }
            logger->debug("Write result for '{}': success=false output=''", segment.name);
            continue;
        }

        core::ExpressionEvaluator evaluator(registry);
        std::vector<FunctorInput> inputs;

        // Resolve inputs from context
        for (const auto& exprInput : lastExpr.inputs) {
            if (exprInput.isDynamic) {
                // Try to resolve from context
                std::string raw = std::get<std::string>(exprInput.value);
                // Simple {key} replacement
                std::string result = raw;
                static const std::regex varPattern(R"(\{([^}:]+)(?::([^}]+))?\})");
                std::smatch match;
                while (std::regex_search(result, match, varPattern)) {
                    std::string key = match[1].str();
                    std::string formatSpec = match[2].matched ? match[2].str() : "";
                    try {
                        auto val = core::getNestedValue(context, key);
                        auto formatted = formatTemplateScalar(val, formatSpec);
                        if (!formatted.has_value()) {
                            break;
                        }
                        result = match.prefix().str() + *formatted + match.suffix().str();
                    } catch (...) {
                        break;
                    }
                }
                inputs.push_back(result);
            } else {
                inputs.push_back(exprInput.value);
            }
        }

        auto result = evaluator.evaluateFormatExpression(lastExpr, inputs, functorCtx, schema->config);
        logger->debug("Write result for '{}': success={} output='{}'",
            segment.name, result.success, result.output);
        if (result.success) return result;
    }

    return FormatResult{false, "No format expressions matched", ""};
}

SilexContextResolve GenericResolver::Impl::resolvePathRecursive(
    const std::vector<std::string>& segments,
    size_t segmentIdx,
    const std::vector<std::shared_ptr<SilexSegment>>& schemaSegments,
    const std::string& parentPath,
    ContextMap& context,
    const std::shared_ptr<SilexSchema>& schema,
    const std::string& lastEndpoint,
    const std::string& schemaPath,
    bool deprecatedTraversal) {

    auto logger = core::getLogger(core::LoggerNames::Resolver);

    if (segmentIdx >= segments.size() || schemaSegments.empty()) {
        // Determine if we reached a proper endpoint
        ResolverStatus status = segmentIdx >= segments.size()
            ? ResolverStatus::Success
            : ResolverStatus::Partial;

        auto segmenter = registry.getSegmenter(schema->info.segmenterUid);
        std::string resolvedPath = segmenter
            ? segmenter->joinSegments(schema->info.rootPath,
                std::vector<std::string>(segments.begin(), segments.begin() + segmentIdx))
            : "";

        std::string unresolvedPath;
        if (segmentIdx < segments.size() && segmenter) {
            unresolvedPath = segmenter->joinSegments("",
                std::vector<std::string>(segments.begin() + segmentIdx, segments.end()));
        }

        SilexContextResolve result;
        result.status = status;
        result.resolvedPath = resolvedPath;
        result.unresolvedPath = unresolvedPath.empty() ? std::nullopt : std::optional(unresolvedPath);
        result.context = context;
        result.schemaUid = schema->info.uid;
        if (!lastEndpoint.empty()) {
            result.schemaEndpoint = lastEndpoint;
        }
        if (!schemaPath.empty()) {
            result.schemaEndpointPath = schemaPath;
        }
        result.usedDeprecatedTraversal = deprecatedTraversal;
        return result;
    }

    const std::string& segmentValue = segments[segmentIdx];
    logger->debug("Resolving segment {}: '{}' against {} schema segments",
        segmentIdx, segmentValue, schemaSegments.size());

    // Expand schema segments: lift children of deprecated segments
    // when deprecated traversal is excluded, so their children are
    // accessible at the current level.
    std::vector<std::shared_ptr<SilexSegment>> expandedSegments = schemaSegments;
    // Track which lifted segments came from deprecated parents
    std::set<SilexSegment*> liftedFromDeprecated;
    if (!options.includeDeprecated) {
        for (const auto& seg : schemaSegments) {
            if (hasFlag(seg->flags, SegmentFlags::Deprecated)) {
                // Lift all children of the deprecated segment
                for (const auto& [branchKey, branchSegs] : seg->branches) {
                    for (const auto& child : branchSegs) {
                        expandedSegments.push_back(child);
                        liftedFromDeprecated.insert(child.get());
                    }
                }
            }
        }
    }

    // Try each schema segment
    for (const auto& schemaSeg : expandedSegments) {
        // Skip deprecated unless requested
        if (hasFlag(schemaSeg->flags, SegmentFlags::Deprecated) && !options.includeDeprecated) {
            continue;
        }

        bool isLifted = liftedFromDeprecated.count(schemaSeg.get()) > 0;

        auto segResult = resolveSegmentParse(
            *schemaSeg, segmentValue, parentPath, context, schema);

        if (segResult.status == ResolverStatus::Success ||
            hasFlag(segResult.status, ResolverStatus::Success)) {
            // Deep merge resolved context
            core::deepMerge(context, segResult.context);

            // Track the deepest matched endpoint
            std::string currentEndpoint = lastEndpoint;
            if (!schemaSeg->endpoint.empty()) {
                currentEndpoint = schemaSeg->endpoint;
            }

            // Build schema path (dot-separated segment names)
            std::string currentSchemaPath = schemaPath;
            if (!schemaSeg->name.empty()) {
                currentSchemaPath = currentSchemaPath.empty()
                    ? schemaSeg->name
                    : currentSchemaPath + "." + schemaSeg->name;
            }
            bool currentDeprecated = deprecatedTraversal || isLifted
                || hasFlag(schemaSeg->flags, SegmentFlags::Deprecated);

            // Build new parent path
            std::string newParent = parentPath;
            auto segmenter = registry.getSegmenter(schema->info.segmenterUid);
            if (segmenter) {
                newParent = segmenter->joinSegments(parentPath, {segmentValue});
            }

            // Find children to recurse into
            std::vector<std::shared_ptr<SilexSegment>> children;
            if (schemaSeg->branches.count("")) {
                children = schemaSeg->branches.at("");
            }

            // Check switch branches
            if (schemaSeg->branches.size() > 1 || !schemaSeg->branches.count("")) {
                // Determine the correct branch based on the matched value.
                // For switch segments, only try the branch whose key matches
                // the segment value (case-insensitive) to avoid false positives.
                std::string valueLower = segmentValue;
                std::transform(valueLower.begin(), valueLower.end(), valueLower.begin(),
                    [](unsigned char c) { return std::tolower(c); });

                // First try exact/case-insensitive match
                std::string matchedBranch;
                for (const auto& [branchKey, branchSegs] : schemaSeg->branches) {
                    if (branchKey == "") continue;
                    std::string keyLower = branchKey;
                    std::transform(keyLower.begin(), keyLower.end(), keyLower.begin(),
                        [](unsigned char c) { return std::tolower(c); });
                    if (keyLower == valueLower) {
                        matchedBranch = branchKey;
                        break;
                    }
                }

                if (!matchedBranch.empty()) {
                    // Only try the matched branch
                    const auto& branchSegs = schemaSeg->branches.at(matchedBranch);
                    ContextMap branchCtx = context;
                    auto branchResult = resolvePathRecursive(
                        segments, segmentIdx + 1, branchSegs,
                        newParent, branchCtx, schema, currentEndpoint,
                        currentSchemaPath, currentDeprecated);
                    if (hasFlag(branchResult.status, ResolverStatus::Success)) {
                        return branchResult;
                    }
                } else {
                    // No direct match — try all branches
                    for (const auto& [branchKey, branchSegs] : schemaSeg->branches) {
                        if (branchKey == "") continue;
                        ContextMap branchCtx = context;
                        auto branchResult = resolvePathRecursive(
                            segments, segmentIdx + 1, branchSegs,
                            newParent, branchCtx, schema, currentEndpoint,
                            currentSchemaPath, currentDeprecated);
                        if (hasFlag(branchResult.status, ResolverStatus::Success)) {
                            return branchResult;
                        }
                    }
                }
            }

            // Recurse into default children
            return resolvePathRecursive(
                segments, segmentIdx + 1, children,
                newParent, context, schema, currentEndpoint,
                currentSchemaPath, currentDeprecated);
        }
    }

    // No segment matched
    SilexContextResolve result;
    result.status = ResolverStatus::Partial;
    result.context = context;
    result.schemaUid = schema->info.uid;
    return result;
}

SilexPathResolve GenericResolver::Impl::writePathRecursive(
    const std::vector<std::shared_ptr<SilexSegment>>& schemaSegments,
    const std::string& parentPath,
    const ContextMap& context,
    const std::shared_ptr<SilexSchema>& schema,
    const std::optional<std::vector<std::string>>& targetEndpoint,
    int depth,
    const std::string& schemaPath,
    bool deprecatedTraversal) {

    auto logger = core::getLogger(core::LoggerNames::Resolver);

    auto flattenMatches = [](const SilexPathResolve& result) {
        std::vector<SilexResolveMatch> flattened;
        if (!result.matches.empty()) {
            flattened = result.matches;
        } else if (result.resolvedPath.has_value()) {
            SilexResolveMatch match;
            match.status = result.status;
            match.resolvedPath = result.resolvedPath;
            match.context = result.context;
            match.schemaUid = result.schemaUid;
            match.schemaEndpoint = result.schemaEndpoint;
            match.schemaEndpointPath = result.schemaEndpointPath;
            match.usedDeprecatedTraversal = result.usedDeprecatedTraversal;
            flattened.push_back(match);
        }
        return flattened;
    };

    auto dedupeMatches = [](std::vector<SilexResolveMatch> matches) {
        std::vector<SilexResolveMatch> deduped;
        std::set<std::string> seenPaths;
        for (auto& match : matches) {
            const std::string key = match.resolvedPath.value_or("");
            if (key.empty() || seenPaths.insert(key).second) {
                deduped.push_back(std::move(match));
            }
        }
        return deduped;
    };

    if (schemaSegments.empty() || depth > 100) {
        SilexPathResolve result;
        // If a target endpoint was specified but not reached, report partial
        result.status = targetEndpoint ? ResolverStatus::Partial : ResolverStatus::Success;
        result.resolvedPath = parentPath;
        result.context = context;
        result.schemaUid = schema->info.uid;
        return result;
    }

    // Track the deepest partial result across all sibling attempts
    SilexPathResolve bestPartial;
    bestPartial.status = ResolverStatus::Partial;
    bestPartial.resolvedPath = parentPath;
    bestPartial.context = context;

    // Collect deprecated segments for fallback lifting
    std::vector<std::shared_ptr<SilexSegment>> deprecatedSegs;

    // When preferDeprecated is set, check if any sibling at this level is deprecated.
    // If so, skip non-deprecated segments to force the deprecated path.
    bool hasDeprecatedSiblings = false;
    if (preferDeprecated) {
        for (const auto& seg : schemaSegments) {
            if (hasFlag(seg->flags, SegmentFlags::Deprecated)) {
                hasDeprecatedSiblings = true;
                break;
            }
        }
    }

    for (const auto& schemaSeg : schemaSegments) {
        // Collect deprecated segments for later fallback
        if (hasFlag(schemaSeg->flags, SegmentFlags::Deprecated) && !options.includeDeprecated) {
            deprecatedSegs.push_back(schemaSeg);
            continue;
        }
        // When preferDeprecated, skip non-deprecated segments at levels with deprecated siblings
        if (preferDeprecated && hasDeprecatedSiblings &&
            !hasFlag(schemaSeg->flags, SegmentFlags::Deprecated)) {
            continue;
        }
        if (hasFlag(schemaSeg->flags, SegmentFlags::ReadOnly) && !contextContainsPlaceholder(context)) {
            continue;
        }

        // Try write expressions
        auto formatResult = evaluateFormatExpressions(*schemaSeg, context, parentPath, schema);
        std::optional<std::string> selectedBranchKey;

        std::vector<std::string> candidateBranchKeys;
        for (const auto& [branchKey, _branchSegs] : schemaSeg->branches) {
            if (branchKey != "") {
                candidateBranchKeys.push_back(branchKey);
            }
        }
        for (const auto& [branchKey, _endpoint] : schemaSeg->branchEndpoints) {
            if (branchKey == "") {
                continue;
            }
            if (std::find(candidateBranchKeys.begin(), candidateBranchKeys.end(), branchKey) ==
                candidateBranchKeys.end()) {
                candidateBranchKeys.push_back(branchKey);
            }
        }

        // For switch-style segments, try evaluating the write expression with
        // {option} bound to the selected branch key.
        if (!formatResult.success && !candidateBranchKeys.empty()) {
            auto tryBranchOptionWrite = [&](const std::string& branchKey) -> bool {
                if (branchKey == "") {
                    return false;
                }

                ContextMap optionContext = context;
                optionContext["option"] = std::any(branchKey);

                auto optionFormatResult = evaluateFormatExpressions(
                    *schemaSeg, optionContext, parentPath, schema);
                if (!optionFormatResult.success) {
                    return false;
                }

                formatResult = optionFormatResult;
                selectedBranchKey = branchKey;
                return true;
            };

            bool branchWriteResolved = false;

            if (targetEndpoint) {
                for (const auto& branchKey : candidateBranchKeys) {
                    auto endpointIt = schemaSeg->branchEndpoints.find(branchKey);
                    if (endpointIt == schemaSeg->branchEndpoints.end()) {
                        continue;
                    }

                    bool branchMatchesTarget = false;
                    for (const auto& endpointValue : *targetEndpoint) {
                        if (endpointMatches(endpointValue, endpointIt->second)) {
                            branchMatchesTarget = true;
                            break;
                        }
                    }

                    if (branchMatchesTarget && tryBranchOptionWrite(branchKey)) {
                        branchWriteResolved = true;
                        break;
                    }
                }
            }

            if (!branchWriteResolved) {
                for (const auto& branchKey : candidateBranchKeys) {
                    if (tryBranchOptionWrite(branchKey)) {
                        break;
                    }
                }
            }
        }

        // If no write expression matched but segment has partitions, evaluate them
        if (!formatResult.success && hasFlag(schemaSeg->flags, SegmentFlags::HasPartitions) &&
            !schemaSeg->partitions.empty()) {
            std::vector<std::string> partOutputs;

            // Helper: get first raw write expression from a partition
            auto getPartWriteExpr = [](const SilexSegment& part) -> std::string {
                for (const auto& formatEntry : part.format) {
                    auto exprIt = formatEntry.find("expressions");
                    if (exprIt == formatEntry.end()) continue;
                    auto* exprs = std::any_cast<std::vector<std::string>>(
                        &exprIt->second);
                    if (exprs && !exprs->empty()) return (*exprs)[0];
                }
                return "";
            };

            // Collect context keys claimed by non-fallback partitions
            std::set<std::string> claimedOptionKeys;
            for (const auto& part : schemaSeg->partitions) {
                if (hasFlag(part->flags, SegmentFlags::Fallback)) continue;
                for (const auto& parseEntry : part->parse) {
                    auto tgtIt = parseEntry.find("targets");
                    if (tgtIt == parseEntry.end()) continue;
                    auto* tgts = std::any_cast<std::map<std::string, std::any>>(
                        &tgtIt->second);
                    if (!tgts) continue;
                    for (const auto& [key, _] : *tgts) {
                        if (key.size() > 8 && key.substr(0, 8) == "options.") {
                            claimedOptionKeys.insert(key.substr(8));
                        }
                    }
                }
                for (const auto& [key, _] : part->targets) {
                    if (key.size() > 8 && key.substr(0, 8) == "options.") {
                        claimedOptionKeys.insert(key.substr(8));
                    }
                }
            }

            // Array expansion regex: matches "prefix{key[*]}"
            static const std::regex arrayExpPat(R"(^(.+)\{([^}\[]+)\[\*\]\}$)");

            // First pass: non-fallback partitions
            for (const auto& part : schemaSeg->partitions) {
                if (hasFlag(part->flags, SegmentFlags::Deprecated) &&
                    !options.includeDeprecated) {
                    continue;
                }
                if (hasFlag(part->flags, SegmentFlags::Fallback)) continue;

                std::string writeExpr = getPartWriteExpr(*part);
                std::smatch am;
                if (std::regex_match(writeExpr, am, arrayExpPat)) {
                    // Array expansion: e.g. "tag={options.tags[*]}"
                    std::string prefix = am[1].str();
                    std::string key = am[2].str();
                    try {
                        auto val = core::getNestedValue(context, key);
                        if (auto* vec = std::any_cast<std::vector<std::any>>(&val)) {
                            for (const auto& item : *vec) {
                                auto formatted = formatTemplateScalar(item);
                                if (formatted) {
                                    partOutputs.push_back(prefix + *formatted);
                                }
                            }
                        }
                    } catch (...) {}
                    continue;
                }

                auto partResult = evaluateFormatExpressions(
                    *part, context, parentPath, schema);
                if (partResult.success && !partResult.output.empty()) {
                    partOutputs.push_back(partResult.output);
                }
            }

            // Second pass: fallback partitions
            for (const auto& part : schemaSeg->partitions) {
                if (hasFlag(part->flags, SegmentFlags::Deprecated) &&
                    !options.includeDeprecated) {
                    continue;
                }
                if (!hasFlag(part->flags, SegmentFlags::Fallback)) continue;

                std::string writeExpr = getPartWriteExpr(*part);
                if (writeExpr == "$K=$V") {
                    // Iterate over unclaimed context keys under "options"
                    try {
                        auto optionsVal = core::getNestedValue(context, "options");
                        if (auto* optionsMap =
                                std::any_cast<ContextMap>(&optionsVal)) {
                            for (const auto& [key, val] : *optionsMap) {
                                if (claimedOptionKeys.count(key)) continue;
                                auto formatted = formatTemplateScalar(val);
                                if (formatted) {
                                    partOutputs.push_back(
                                        key + "=" + *formatted);
                                }
                            }
                        }
                    } catch (...) {}
                    continue;
                }

                auto partResult = evaluateFormatExpressions(
                    *part, context, parentPath, schema);
                if (partResult.success && !partResult.output.empty()) {
                    partOutputs.push_back(partResult.output);
                }
            }

            if (!partOutputs.empty()) {
                auto partSegmenter = schemaSeg->partitionSegmenter.has_value()
                    ? registry.getSegmenter(schemaSeg->partitionSegmenter.value())
                    : std::shared_ptr<silex::ISegmenter>(nullptr);
                if (partSegmenter) {
                    formatResult = FormatResult{true, "Success", partSegmenter->joinSegments("", partOutputs)};
                } else {
                    formatResult = FormatResult{true, "Success", "?" + core::joinStrings(partOutputs, "&")};
                }
            }
        }

        if (formatResult.success) {
            auto segmenter = registry.getSegmenter(schema->info.segmenterUid);
            const std::vector<std::string> candidateOutputs = formatResult.matches.empty()
                ? std::vector<std::string>{formatResult.output}
                : formatResult.matches;

            std::vector<SilexResolveMatch> successfulMatches;

            for (const auto& candidateOutput : candidateOutputs) {
                std::string newPath = segmenter
                    ? segmenter->joinSegments(parentPath, {candidateOutput})
                    : parentPath + "/" + candidateOutput;

                // Track schema path and deprecated state
                std::string currentSchemaPath = schemaPath.empty()
                    ? schemaSeg->name
                    : schemaPath + "." + schemaSeg->name;
                bool currentDeprecated = deprecatedTraversal
                    || hasFlag(schemaSeg->flags, SegmentFlags::Deprecated);

                logger->debug("Write path for '{}': '{}' -> '{}'",
                    schemaSeg->name, parentPath, newPath);

                // Check if we've reached the target endpoint
                if (targetEndpoint && !schemaSeg->endpoint.empty()) {
                    bool endpointMatch = false;
                    for (const auto& ep : *targetEndpoint) {
                        if (endpointMatches(ep, schemaSeg->endpoint)) {
                            endpointMatch = true;
                            break;
                        }
                    }
                    if (endpointMatch) {
                        // Try children first (for query params, etc.)
                        std::vector<std::shared_ptr<SilexSegment>> epChildren;
                        if (schemaSeg->branches.count("")) {
                            epChildren = schemaSeg->branches.at("");
                        }
                        if (!epChildren.empty()) {
                            auto childResult = writePathRecursive(
                                epChildren, newPath, context, schema, std::nullopt, depth + 1,
                                currentSchemaPath, currentDeprecated);
                            if (hasFlag(childResult.status, ResolverStatus::Success)) {
                                childResult.schemaEndpoint = schemaSeg->endpoint;
                                childResult.schemaUid = schema->info.uid;
                                if (!childResult.schemaEndpointPath) childResult.schemaEndpointPath = currentSchemaPath;
                                childResult.usedDeprecatedTraversal = childResult.usedDeprecatedTraversal || currentDeprecated;
                                auto childMatches = flattenMatches(childResult);
                                for (auto& childMatch : childMatches) {
                                    childMatch.schemaUid = schema->info.uid;
                                    childMatch.schemaEndpoint = schemaSeg->endpoint;
                                    childMatch.schemaEndpointPath = currentSchemaPath;
                                    childMatch.usedDeprecatedTraversal =
                                        childMatch.usedDeprecatedTraversal || currentDeprecated;
                                }
                                successfulMatches.insert(successfulMatches.end(), childMatches.begin(), childMatches.end());
                            }
                        }
                        SilexResolveMatch match;
                        match.status = ResolverStatus::Success;
                        match.resolvedPath = newPath;
                        match.context = context;
                        match.schemaUid = schema->info.uid;
                        match.schemaEndpoint = schemaSeg->endpoint;
                        match.schemaEndpointPath = currentSchemaPath;
                        match.usedDeprecatedTraversal = currentDeprecated;
                        successfulMatches.push_back(match);
                        continue;
                    }
                }

                // Recurse into default children first
                std::vector<std::shared_ptr<SilexSegment>> children;
                if (schemaSeg->branches.count("")) {
                    children = schemaSeg->branches.at("");
                }

                logger->debug("Segment '{}' has {} branches, {} default children",
                    schemaSeg->name, schemaSeg->branches.size(), children.size());

                if (selectedBranchKey) {
                    auto endpointIt = schemaSeg->branchEndpoints.find(*selectedBranchKey);
                    bool hasBranchChildren = schemaSeg->branches.count(*selectedBranchKey) > 0;
                    if (targetEndpoint && endpointIt != schemaSeg->branchEndpoints.end() && !hasBranchChildren) {
                        for (const auto& ep : *targetEndpoint) {
                            if (endpointMatches(ep, endpointIt->second)) {
                                SilexResolveMatch match;
                                match.status = ResolverStatus::Success;
                                match.resolvedPath = newPath;
                                match.context = context;
                                match.schemaUid = schema->info.uid;
                                match.schemaEndpoint = endpointIt->second;
                                match.schemaEndpointPath = currentSchemaPath;
                                match.usedDeprecatedTraversal = currentDeprecated;
                                successfulMatches.push_back(match);
                                break;
                            }
                        }
                        continue;
                    }
                }

                // Try switch branches - find which branch leads to the target endpoint
                bool branchMatched = false;
                for (const auto& [branchKey, branchSegs] : schemaSeg->branches) {
                    if (branchKey == "") continue;
                    if (selectedBranchKey && branchKey != *selectedBranchKey) {
                        continue;
                    }

                    // Check if this branch itself has an endpoint that matches target
                    if (targetEndpoint) {
                        auto bepIt = schemaSeg->branchEndpoints.find(branchKey);
                        if (bepIt != schemaSeg->branchEndpoints.end()) {
                            for (const auto& ep : *targetEndpoint) {
                                if (endpointMatches(ep, bepIt->second)) {
                                    logger->debug("Branch '{}' matches endpoint '{}' at path '{}'",
                                        branchKey, ep, newPath);
                                    SilexResolveMatch match;
                                    match.status = ResolverStatus::Success;
                                    match.resolvedPath = newPath;
                                    match.context = context;
                                    match.schemaUid = schema->info.uid;
                                    match.schemaEndpoint = ep;
                                    match.schemaEndpointPath = currentSchemaPath;
                                    match.usedDeprecatedTraversal = currentDeprecated;
                                    successfulMatches.push_back(match);
                                    branchMatched = true;
                                    break;
                                }
                            }
                            if (branchMatched) {
                                break;
                            }
                        }
                    }

                    logger->debug("Trying switch branch '{}' with {} segs", branchKey, branchSegs.size());
                    auto branchResult = writePathRecursive(
                        branchSegs, newPath, context, schema, targetEndpoint, depth + 1,
                        currentSchemaPath, currentDeprecated);
                    logger->debug("Branch '{}' result: status={}, path='{}'",
                        branchKey, static_cast<int>(branchResult.status), branchResult.resolvedPath.value_or(""));
                    if (hasFlag(branchResult.status, ResolverStatus::Success)) {
                        branchResult.usedDeprecatedTraversal = branchResult.usedDeprecatedTraversal || currentDeprecated;
                        auto branchMatches = flattenMatches(branchResult);
                        successfulMatches.insert(successfulMatches.end(), branchMatches.begin(), branchMatches.end());
                        branchMatched = true;
                    }
                }
                if (branchMatched) {
                    continue;
                }

                // Recurse into default children
                if (!children.empty()) {
                    logger->debug("Recursing into {} default children of '{}'", children.size(), schemaSeg->name);
                    auto childResult = writePathRecursive(
                        children, newPath, context, schema, targetEndpoint, depth + 1,
                        currentSchemaPath, currentDeprecated);
                    logger->debug("Default children result: status={}, path='{}'",
                        static_cast<int>(childResult.status), childResult.resolvedPath.value_or(""));
                    if (hasFlag(childResult.status, ResolverStatus::Success)) {
                        childResult.usedDeprecatedTraversal = childResult.usedDeprecatedTraversal || currentDeprecated;
                        auto childMatches = flattenMatches(childResult);
                        successfulMatches.insert(successfulMatches.end(), childMatches.begin(), childMatches.end());
                        continue;
                    }
                }

                // No children matched - this might be the end
                if (children.empty() && schemaSeg->branches.size() <= 1) {
                    if (!targetEndpoint) {
                        SilexResolveMatch match;
                        match.status = ResolverStatus::Success;
                        match.resolvedPath = newPath;
                        match.context = context;
                        match.schemaUid = schema->info.uid;
                        match.schemaEndpointPath = currentSchemaPath;
                        match.usedDeprecatedTraversal = currentDeprecated;
                        successfulMatches.push_back(match);
                        continue;
                    }
                    // Track as best partial and continue trying siblings
                    if (!bestPartial.resolvedPath || newPath.size() > bestPartial.resolvedPath->size()) {
                        bestPartial.resolvedPath = newPath;
                        bestPartial.context = context;
                        bestPartial.schemaUid = schema->info.uid;
                    }
                }
            }

            successfulMatches = dedupeMatches(std::move(successfulMatches));
            if (!successfulMatches.empty()) {
                SilexPathResolve result;
                result.status = ResolverStatus::Success;
                if (successfulMatches.size() > 1) {
                    result.status = result.status | ResolverStatus::Ambiguous;
                }
                result.resolvedPath = successfulMatches.front().resolvedPath;
                result.context = context;
                result.schemaUid = schema->info.uid;
                result.schemaEndpoint = successfulMatches.front().schemaEndpoint;
                result.schemaEndpointPath = successfulMatches.front().schemaEndpointPath;
                result.usedDeprecatedTraversal = successfulMatches.front().usedDeprecatedTraversal;
                result.matches = std::move(successfulMatches);
                return result;
            }
        }
    }

    // Fallback: try children of deprecated segments (lifted)
    // Only lift if the deprecated segment's own write would succeed,
    // indicating the context supports this branch.
    if (!deprecatedSegs.empty() && !hasFlag(bestPartial.status, ResolverStatus::Success)) {
        for (const auto& depSeg : deprecatedSegs) {
            auto depWrite = evaluateFormatExpressions(*depSeg, context, parentPath, schema);
            if (!depWrite.success) continue;

            for (const auto& [branchKey, branchSegs] : depSeg->branches) {
                auto liftedResult = writePathRecursive(
                    branchSegs, parentPath, context, schema,
                    targetEndpoint, depth + 1,
                    schemaPath, true);
                if (hasFlag(liftedResult.status, ResolverStatus::Success)) {
                    liftedResult.usedDeprecatedTraversal = true;
                    return liftedResult;
                }
            }
        }
    }

    return bestPartial;
}

} // namespace resolvers
} // namespace silex
