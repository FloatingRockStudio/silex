// (C) Copyright 2026 Floating Rock Studio Ltd
// SPDX-License-Identifier: MIT

#pragma once

/// @file GlobTagFunctor.h
/// @brief Functor for version/tag-aware file matching (v001, latest, published).

#include <silex/interfaces/IFunctor.h>

#include <map>
#include <string>

namespace silex {
namespace functors {

/// Version/tag-aware file matching functor. Aliases: glob_tag.
/// Supports named tags (published, deprecated), numeric versions, special tags (latest, next).
class GlobTagFunctor : public IFunctor {
public:
    /// Default constructor.
    GlobTagFunctor() = default;
    /// Default destructor.
    ~GlobTagFunctor() override = default;

    /// Resolve a glob-tag pattern to matching filesystem paths.
    ParseResult parse(const std::vector<FunctorInput>& inputs,
                    const std::vector<FunctorOutput>& outputs,
                    const FunctorContext& context) override;
    /// Construct a glob-tag pattern from context values.
    FormatResult format(const std::vector<FunctorInput>& inputs,
                      const FunctorContext& context) override;

private:
    /// Configuration for tag names, version pattern, and separator.
    struct TagConfig {
        std::map<std::string, std::string> namedTags;
        std::string versionPattern = R"(v(\d+))";
        std::string tagSeparator = "_";
    };

    /// Extract tag configuration from the functor context.
    TagConfig getConfig(const FunctorContext& context);
    TagConfig m_defaultConfig;
};

} // namespace functors
} // namespace silex
