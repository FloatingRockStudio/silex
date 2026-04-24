/// @file GlobTagFunctor.cpp
/// @brief Implementation of version/tag-aware file matching functor.

#include "GlobTagFunctor.h"
#include "util/Logging.h"
#include "util/Utils.h"

#include <algorithm>
#include <filesystem>
#include <regex>
#include <variant>

namespace fs = std::filesystem;

namespace silex {
namespace functors {

namespace {

std::string inputToStr(const FunctorInput& input) {
    return std::visit([](const auto& v) -> std::string {
        using T = std::decay_t<decltype(v)>;
        if constexpr (std::is_same_v<T, std::string>) return v;
        else if constexpr (std::is_same_v<T, int>) return std::to_string(v);
        else if constexpr (std::is_same_v<T, double>) return std::to_string(v);
        else if constexpr (std::is_same_v<T, bool>) return v ? "true" : "false";
        else return "";
    }, input);
}

/// Extract version number from a tag string.
int extractVersion(const std::string& tag, const std::string& pattern) {
    std::regex re(pattern);
    std::smatch match;
    if (std::regex_search(tag, match, re) && match.size() > 1) {
        try { return std::stoi(match[1].str()); } catch (...) {}
    }
    return -1;
}

} // anonymous namespace

GlobTagFunctor::TagConfig GlobTagFunctor::getConfig(const FunctorContext& context) {
    TagConfig config = m_defaultConfig;

    // Look for tag configuration in variables
    auto it = context.variables.find("tag_config");
    if (it != context.variables.end()) {
        if (auto* m = std::any_cast<ContextMap>(&it->second)) {
            auto sepIt = m->find("separator");
            if (sepIt != m->end()) {
                if (auto* s = std::any_cast<std::string>(&sepIt->second)) {
                    config.tagSeparator = *s;
                }
            }
            auto patIt = m->find("version_pattern");
            if (patIt != m->end()) {
                if (auto* s = std::any_cast<std::string>(&patIt->second)) {
                    config.versionPattern = *s;
                }
            }
        }
    }

    return config;
}

ParseResult GlobTagFunctor::parse(
    const std::vector<FunctorInput>& inputs,
    const std::vector<FunctorOutput>& outputs,
    const FunctorContext& context) {

    if (inputs.empty()) {
        return ParseResult{false, "No pattern provided", {}};
    }

    std::string pattern = inputToStr(inputs[0]);
    auto config = getConfig(context);

    // Replace %TAG% with * for glob matching, and build regex for version extraction
    std::string globPattern = pattern;
    std::string tagPlaceholder = "%TAG%";
    size_t tagPos = globPattern.find(tagPlaceholder);
    if (tagPos != std::string::npos) {
        globPattern.replace(tagPos, tagPlaceholder.size(), "*");
    }

    // Build regex from pattern: escape everything except %TAG%, replace %TAG% with capture group
    std::string regexStr;
    size_t pos = 0;
    size_t tPos = pattern.find(tagPlaceholder);
    if (tPos != std::string::npos) {
        // Escape the parts before and after %TAG%
        std::string before = pattern.substr(0, tPos);
        std::string after = pattern.substr(tPos + tagPlaceholder.size());
        // Simple regex escape for fixed parts
        auto escapeRegex = [](const std::string& s) {
            std::string escaped;
            for (char c : s) {
                if (c == '.' || c == '*' || c == '+' || c == '?' || c == '(' ||
                    c == ')' || c == '[' || c == ']' || c == '{' || c == '}' ||
                    c == '\\' || c == '^' || c == '$' || c == '|') {
                    escaped += '\\';
                }
                escaped += c;
            }
            return escaped;
        };
        regexStr = escapeRegex(before) + "(.*)" + escapeRegex(after);
    }

    // Glob files matching pattern
    std::vector<std::string> matches;
    std::vector<std::string> matchTags; // extracted tag for each match
    try {
        fs::path base(context.parent);
        if (fs::exists(base) && fs::is_directory(base)) {
            std::regex tagRegex(regexStr.empty() ? ".*" : regexStr);
            for (const auto& entry : fs::directory_iterator(base)) {
                if (entry.is_directory()) continue;
                std::string name = entry.path().filename().string();
                if (name.empty() || name[0] == '.') continue;
                if (core::globMatch(globPattern, name)) {
                    matches.push_back(name);
                    // Extract tag
                    std::smatch sm;
                    if (!regexStr.empty() && std::regex_match(name, sm, tagRegex) && sm.size() > 1) {
                        matchTags.push_back(sm[1].str());
                    } else {
                        matchTags.push_back("");
                    }
                }
            }
        }
    } catch (...) {}

    if (matches.empty()) {
        return ParseResult{false, "No matches for pattern: " + pattern, {}};
    }

    // Find the latest version
    std::string bestMatch;
    std::string bestTag;
    int highestVersion = -1;

    for (size_t mi = 0; mi < matches.size(); ++mi) {
        int version = -1;
        if (!matchTags[mi].empty()) {
            // Try to parse version from the extracted tag
            try { version = std::stoi(matchTags[mi]); } catch (...) {}
            if (version < 0) {
                version = extractVersion(matchTags[mi], config.versionPattern);
            }
        }
        if (version < 0) {
            version = extractVersion(matches[mi], config.versionPattern);
        }
        if (version > highestVersion) {
            highestVersion = version;
            bestMatch = matches[mi];
            bestTag = matchTags[mi];
        }
    }

    if (bestMatch.empty()) {
        bestMatch = matches[0];
        bestTag = matchTags.empty() ? "" : matchTags[0];
    }

    // Discover tags from hidden tag files (e.g. .test_v001.published for test_v001.ma)
    std::vector<std::string> bestTags;
    try {
        fs::path base(context.parent);
        fs::path bestPath(bestMatch);
        std::string tagPrefix = "." + bestPath.stem().string() + ".";
        for (const auto& entry : fs::directory_iterator(base)) {
            std::string name = entry.path().filename().string();
            if (name.size() > tagPrefix.size() && name.substr(0, tagPrefix.size()) == tagPrefix) {
                bestTags.push_back(name.substr(tagPrefix.size()));
            }
        }
    } catch (...) {}

    // Determine output type for each output based on options and name inference
    auto getOutputType = [](const FunctorOutput& output) -> std::string {
        // Options take priority
        for (const auto& opt : output.options) {
            std::string lower = opt;
            std::transform(lower.begin(), lower.end(), lower.begin(),
                [](unsigned char c) { return std::tolower(c); });
            if (lower == "version" || lower == "ver" || lower == "v") return "version";
            if (lower == "name" || lower == "filename" || lower == "file") return "name";
            if (lower == "tags" || lower == "tag_list") return "tags";
            if (lower == "tag") return "tag";
        }
        // Infer from output name
        std::string lowerName = output.name;
        std::transform(lowerName.begin(), lowerName.end(), lowerName.begin(),
            [](unsigned char c) { return std::tolower(c); });
        if (lowerName == "version" || lowerName == "ver" || lowerName == "v") return "version";
        if (lowerName == "tags" || lowerName == "tag_list" || lowerName.find("tags") != std::string::npos) return "tags";
        if (lowerName == "tag" || lowerName == "single_tag") return "tag";
        if (lowerName == "name" || lowerName == "filename" || lowerName == "file") return "name";
        return "name"; // Default to filename
    };

    std::map<std::string, ResolvedValue> resultOutputs;
    for (const auto& output : outputs) {
        std::string type = getOutputType(output);
        if (type == "version") {
            resultOutputs[output.name] = ResolvedValue{highestVersion, false};
        } else if (type == "tags") {
            resultOutputs[output.name] = ResolvedValue{
                std::vector<std::any>(bestTags.begin(), bestTags.end()), false};
        } else if (type == "tag") {
            std::string firstTag = bestTags.empty() ? "" : bestTags[0];
            resultOutputs[output.name] = ResolvedValue{firstTag, false};
        } else {
            // name/filename/default
            resultOutputs[output.name] = ResolvedValue{bestMatch, matches.size() > 1};
        }
    }

    return ParseResult{true, "Success", resultOutputs};
}

FormatResult GlobTagFunctor::format(
    const std::vector<FunctorInput>& inputs,
    const FunctorContext& context) {

    if (inputs.empty()) {
        return FormatResult{false, "No input provided", ""};
    }

    std::string pattern = inputToStr(inputs[0]);
    auto config = getConfig(context);

    // Replace %TAG% in pattern for matching
    std::string tagPlaceholder = "%TAG%";
    size_t tagPos = pattern.find(tagPlaceholder);

    // Get tag configuration from variables
    struct TagInfo {
        bool isExplicit = false;
        std::vector<std::string> aliases;
    };
    std::map<std::string, TagInfo> tagDefs;

    auto tagsIt = context.variables.find("tags");
    if (tagsIt != context.variables.end()) {
        if (auto* tm = std::any_cast<ContextMap>(&tagsIt->second)) {
            for (const auto& [tagName, tagVal] : *tm) {
                TagInfo info;
                if (auto* tagMap = std::any_cast<ContextMap>(&tagVal)) {
                    auto expIt = tagMap->find("explicit");
                    if (expIt != tagMap->end()) {
                        if (auto* b = std::any_cast<bool>(&expIt->second)) info.isExplicit = *b;
                    }
                    auto aliasIt = tagMap->find("alias");
                    if (aliasIt != tagMap->end()) {
                        if (auto* vs = std::any_cast<std::vector<std::string>>(&aliasIt->second)) {
                            info.aliases = *vs;
                        } else if (auto* va = std::any_cast<std::vector<std::any>>(&aliasIt->second)) {
                            for (const auto& a : *va) {
                                if (auto* s = std::any_cast<std::string>(&a)) info.aliases.push_back(*s);
                            }
                        }
                    }
                }
                tagDefs[tagName] = info;
            }
        }
    }

    // Build glob matching pattern
    std::string globPattern = pattern;
    if (tagPos != std::string::npos) {
        globPattern = pattern.substr(0, tagPos) + "*" + pattern.substr(tagPos + tagPlaceholder.size());
    }

    // Build regex for tag extraction
    std::string regexStr;
    if (tagPos != std::string::npos) {
        auto escapeRegex = [](const std::string& s) {
            std::string escaped;
            for (char c : s) {
                if (c == '.' || c == '*' || c == '+' || c == '?' || c == '(' ||
                    c == ')' || c == '[' || c == ']' || c == '{' || c == '}' ||
                    c == '\\' || c == '^' || c == '$' || c == '|') {
                    escaped += '\\';
                }
                escaped += c;
            }
            return escaped;
        };
        regexStr = escapeRegex(pattern.substr(0, tagPos)) + "(.*)" + escapeRegex(pattern.substr(tagPos + tagPlaceholder.size()));
    }

    // Gather matching files
    struct FileEntry {
        std::string filename;
        std::string tag;
        int version = -1;
        std::vector<std::string> fileTags;
    };
    std::vector<FileEntry> entries;

    try {
        fs::path base(context.parent);
        if (fs::exists(base) && fs::is_directory(base)) {
            std::regex tagRegex(regexStr.empty() ? ".*" : regexStr);

            for (const auto& entry : fs::directory_iterator(base)) {
                if (entry.is_directory()) continue;
                std::string name = entry.path().filename().string();
                if (name.empty() || name[0] == '.') continue;
                if (core::globMatch(globPattern, name)) {
                    FileEntry fe;
                    fe.filename = name;
                    std::smatch sm;
                    if (!regexStr.empty() && std::regex_match(name, sm, tagRegex) && sm.size() > 1) {
                        fe.tag = sm[1].str();
                        try { fe.version = std::stoi(fe.tag); } catch (...) {}
                        if (fe.version < 0) fe.version = extractVersion(fe.tag, config.versionPattern);
                    } else {
                        fe.version = extractVersion(name, config.versionPattern);
                    }
                    entries.push_back(fe);
                }
            }

            // Discover tags for each entry
            for (auto& fe : entries) {
                // Tag files use stem (no extension): .test_v001.published for test_v001.ma
                fs::path fePath(fe.filename);
                std::string stem = fePath.stem().string();
                std::string tagPrefix = "." + stem + ".";
                for (const auto& dentry : fs::directory_iterator(base)) {
                    std::string dname = dentry.path().filename().string();
                    if (dname.size() > tagPrefix.size() && dname.substr(0, tagPrefix.size()) == tagPrefix) {
                        fe.fileTags.push_back(dname.substr(tagPrefix.size()));
                    }
                }
            }
        }
    } catch (...) {}

    // If no entries found
    if (entries.empty()) {
        // If we're generating a version number from a numeric input, we can still produce output
        if (inputs.size() > 1) {
            std::string tagValue = inputToStr(inputs[1]);
            bool isNumeric = !tagValue.empty();
            for (char c : tagValue) { if (!std::isdigit(static_cast<unsigned char>(c))) { isNumeric = false; break; } }
            if (isNumeric && tagPos != std::string::npos) {
                int padding = 3;
                if (inputs.size() > 2) {
                    std::string padStr = inputToStr(inputs[2]);
                    try { padding = std::stoi(padStr); } catch (...) {}
                }
                int version = std::stoi(tagValue);
                std::string vStr = std::to_string(version);
                while (static_cast<int>(vStr.size()) < padding) vStr = "0" + vStr;
                std::string result = pattern;
                result.replace(result.find(tagPlaceholder), tagPlaceholder.size(), vStr);
                return FormatResult{true, "Success", result};
            }
            // Handle "next" with no existing files → version 1
            std::string lowerTag = tagValue;
            std::transform(lowerTag.begin(), lowerTag.end(), lowerTag.begin(),
                [](unsigned char c) { return std::tolower(c); });
            if (lowerTag == "next" && tagPos != std::string::npos) {
                int padding = 4;
                if (inputs.size() > 2) {
                    std::string padStr = inputToStr(inputs[2]);
                    try { padding = std::stoi(padStr); } catch (...) {}
                }
                std::string vStr = "1";
                while (static_cast<int>(vStr.size()) < padding) vStr = "0" + vStr;
                std::string result = pattern;
                result.replace(result.find(tagPlaceholder), tagPlaceholder.size(), vStr);
                return FormatResult{true, "Success", result};
            }
        }
        return FormatResult{false, "No files found matching pattern: " + pattern, ""};
    }

    // Second input: tag name or version number
    if (inputs.size() < 2) {
        // No tag specified, return latest version file
        std::string bestMatch;
        int highest = -1;
        for (const auto& fe : entries) {
            if (fe.version > highest) { highest = fe.version; bestMatch = fe.filename; }
        }
        return FormatResult{true, "Success", bestMatch.empty() ? entries[0].filename : bestMatch};
    }

    std::string tagValue = inputToStr(inputs[1]);

    // Check if tag value is numeric (version number)
    bool isNumeric = !tagValue.empty();
    for (char c : tagValue) { if (!std::isdigit(static_cast<unsigned char>(c))) { isNumeric = false; break; } }

    if (isNumeric && tagPos != std::string::npos) {
        // Version generation: format version with padding
        int padding = 3;
        if (inputs.size() > 2) {
            std::string padStr = inputToStr(inputs[2]);
            try { padding = std::stoi(padStr); } catch (...) {}
        }
        int version = std::stoi(tagValue);
        std::string vStr = std::to_string(version);
        while (static_cast<int>(vStr.size()) < padding) vStr = "0" + vStr;
        std::string result = pattern;
        result.replace(result.find(tagPlaceholder), tagPlaceholder.size(), vStr);
        return FormatResult{true, "Success", result};
    }

    // Helper: check if file has an explicit tag
    auto hasExplicitTag = [&](const FileEntry& fe) {
        for (const auto& ft : fe.fileTags) {
            auto it = tagDefs.find(ft);
            if (it != tagDefs.end() && it->second.isExplicit) return true;
        }
        return false;
    };

    // Resolve tag request
    std::string tagRequest = tagValue;
    std::transform(tagRequest.begin(), tagRequest.end(), tagRequest.begin(),
        [](unsigned char c) { return std::tolower(c); });

    // Build ordered list of tags to try (for composite tags, use aliases)
    std::vector<std::string> tagsToTry;
    auto defIt = tagDefs.find(tagRequest);
    if (defIt != tagDefs.end() && !defIt->second.aliases.empty()) {
        tagsToTry = defIt->second.aliases;
    } else {
        tagsToTry.push_back(tagRequest);
    }

    // Try each tag in order
    for (const auto& tryTag : tagsToTry) {
        if (tryTag == "latest") {
            // Find highest version without explicit tags
            std::string bestMatch;
            int highest = -1;
            for (const auto& fe : entries) {
                if (hasExplicitTag(fe)) continue;
                if (fe.version > highest) { highest = fe.version; bestMatch = fe.filename; }
            }
            if (!bestMatch.empty()) {
                return FormatResult{true, "Success", bestMatch};
            }
            continue;
        }
        if (tryTag == "next") {
            // Find highest version and increment
            int highest = 0;
            int maxPadding = 3;
            for (const auto& fe : entries) {
                if (fe.version > highest) {
                    highest = fe.version;
                    maxPadding = static_cast<int>(fe.tag.size());
                }
            }
            highest++;
            int padding = 3;
            if (inputs.size() > 2) {
                std::string padStr = inputToStr(inputs[2]);
                try { padding = std::stoi(padStr); } catch (...) {}
            } else if (maxPadding > 0) {
                padding = maxPadding;
            }
            std::string vStr = std::to_string(highest);
            while (static_cast<int>(vStr.size()) < padding) vStr = "0" + vStr;
            std::string result = pattern;
            result.replace(result.find(tagPlaceholder), tagPlaceholder.size(), vStr);
            return FormatResult{true, "Success", result};
        }

        // Named tag: find file with this tag
        // If the requested tag is not explicit, skip files that have any explicit tag
        bool requestedIsExplicit = false;
        auto reqDefIt = tagDefs.find(tryTag);
        if (reqDefIt != tagDefs.end()) requestedIsExplicit = reqDefIt->second.isExplicit;

        // Sort by version descending so we get latest version with this tag
        std::vector<const FileEntry*> matchingEntries;
        for (const auto& fe : entries) {
            bool hasTag = false;
            for (const auto& ft : fe.fileTags) {
                if (ft == tryTag) { hasTag = true; break; }
            }
            if (!hasTag) continue;

            // If non-explicit request, skip files with explicit tags
            if (!requestedIsExplicit && hasExplicitTag(fe)) continue;

            matchingEntries.push_back(&fe);
        }
        if (!matchingEntries.empty()) {
            // Return the one with highest version
            const FileEntry* best = matchingEntries[0];
            for (const auto* fe : matchingEntries) {
                if (fe->version > best->version) best = fe;
            }
            return FormatResult{true, "Success", best->filename};
        }
    }

    return FormatResult{false, "No files found with tags: " + tagValue, ""};
}

} // namespace functors
} // namespace silex
