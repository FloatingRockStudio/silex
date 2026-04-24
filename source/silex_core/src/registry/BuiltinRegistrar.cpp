// (C) Copyright 2026 Floating Rock Studio Ltd
// SPDX-License-Identifier: MIT

/// @file BuiltinRegistrar.cpp
/// @brief Registers all built-in functors and segmenters.

#include "BuiltinRegistrar.h"
#include "Registry.h"

#include "functors/CaseConversionFunctor.h"
#include "functors/CaseSplitFunctor.h"
#include "functors/GlobFunctor.h"
#include "functors/GlobTagFunctor.h"
#include "functors/LexiconFunctor.h"
#include "segmenters/FilesystemSegmenter.h"
#include "segmenters/URISegmenter.h"

namespace silex {
namespace core {

void registerBuiltins(Registry& registry) {
    // MARK: Functors

    auto regFunctor = [&](const std::string& uid,
                          const std::string& name,
                          const std::string& module,
                          const std::vector<std::string>& aliases,
                          FunctorFactory factory) {
        SilexFunctorInfo info;
        info.uid = uid;
        info.name = name;
        info.module = module;
        info.package = "silex";
        info.language = Language::Other; // C++ native
        info.aliases = aliases;
        registry.registerFunctor(info, std::move(factory));
    };

    regFunctor(
        "silex.impl.functors.case_conversion.ConvertLowerCaseFunctor",
        "ConvertLowerCaseFunctor",
        "silex.impl.functors.case_conversion",
        {"lower_case", "lowercase", "lower"},
        [] { return std::make_shared<functors::ConvertLowerCaseFunctor>(); });

    regFunctor(
        "silex.impl.functors.case_conversion.ConvertUpperCaseFunctor",
        "ConvertUpperCaseFunctor",
        "silex.impl.functors.case_conversion",
        {"upper_case", "uppercase"},
        [] { return std::make_shared<functors::ConvertUpperCaseFunctor>(); });

    regFunctor(
        "silex.impl.functors.case_conversion.ConvertTitleCaseFunctor",
        "ConvertTitleCaseFunctor",
        "silex.impl.functors.case_conversion",
        {"title_case", "titlecase", "title"},
        [] { return std::make_shared<functors::ConvertTitleCaseFunctor>(); });

    regFunctor(
        "silex.impl.functors.case_split.SplitCamelCaseFunctor",
        "SplitCamelCaseFunctor",
        "silex.impl.functors.case_split",
        {"camelcase", "CC"},
        [] { return std::make_shared<functors::SplitCamelCaseFunctor>(); });

    regFunctor(
        "silex.impl.functors.case_split.SplitSnakeCaseFunctor",
        "SplitSnakeCaseFunctor",
        "silex.impl.functors.case_split",
        {"snakecase", "SC"},
        [] { return std::make_shared<functors::SplitSnakeCaseFunctor>(); });

    regFunctor(
        "silex.impl.functors.glob.GlobFunctor",
        "GlobFunctor",
        "silex.impl.functors.glob",
        {"glob"},
        [] { return std::make_shared<functors::GlobFunctor>(); });

    regFunctor(
        "silex.impl.functors.glob_tag.GlobTagFunctor",
        "GlobTagFunctor",
        "silex.impl.functors.glob_tag",
        {"glob_tag"},
        [] { return std::make_shared<functors::GlobTagFunctor>(); });

    regFunctor(
        "silex.impl.functors.lexicon.LexiconFunctor",
        "LexiconFunctor",
        "silex.impl.functors.lexicon",
        {"lexicon", "L"},
        [] { return std::make_shared<functors::LexiconFunctor>(); });

    // MARK: Segmenters

    auto regSegmenter = [&](const std::string& uid,
                            const std::string& name,
                            const std::string& module,
                            SegmenterFactory factory) {
        ExternalResource info;
        info.uid = uid;
        info.name = name;
        info.module = module;
        info.package = "silex";
        info.language = Language::Other;
        registry.registerSegmenter(info, std::move(factory));
    };

    regSegmenter(
        "silex.impl.segmenters.FilesystemSegmenter",
        "FilesystemSegmenter",
        "silex.impl.segmenters",
        [] { return std::make_shared<segmenters::FilesystemSegmenter>(); });

    regSegmenter(
        "silex.impl.segmenters.uri.URISegmenter",
        "URISegmenter",
        "silex.impl.segmenters.uri",
        [] { return std::make_shared<segmenters::URISegmenter>(); });

    regSegmenter(
        "silex.impl.segmenters.uri.QueryParamsSegmenter",
        "QueryParamsSegmenter",
        "silex.impl.segmenters.uri",
        [] { return std::make_shared<segmenters::QueryParamsSegmenter>(); });
}

} // namespace core
} // namespace silex
