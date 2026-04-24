// (C) Copyright 2026 Floating Rock Studio Ltd
// SPDX-License-Identifier: MIT

#pragma once

/// @file LexiconFunctor.h
/// @brief Functor for abbreviation-to-full-name mapping and reverse lookup.

#include <silex/interfaces/IFunctor.h>

namespace silex {
namespace functors {

/// Abbreviation to full name conversion functor. Aliases: lexicon, L.
/// READ: abbreviation -> full name (e.g., "chr" -> "character")
/// WRITE: full name -> abbreviation (e.g., "character" -> "chr")
class LexiconFunctor : public IFunctor {
public:
    /// Look up a segment value in the lexicon mapping on read.
    ParseResult parse(const std::vector<FunctorInput>& inputs,
                    const std::vector<FunctorOutput>& outputs,
                    const FunctorContext& context) override;
    /// Reverse-lookup a context value in the lexicon mapping on write.
    FormatResult format(const std::vector<FunctorInput>& inputs,
                      const FunctorContext& context) override;
};

} // namespace functors
} // namespace silex
