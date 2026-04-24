#pragma once

/// @file GlobFunctor.h
/// @brief Functor for filesystem glob pattern matching with session-scoped caching.

#include <silex/interfaces/IFunctor.h>

#include <string>
#include <unordered_map>

namespace silex {
namespace functors {

/// File pattern matching functor. Aliases: glob.
class GlobFunctor : public IFunctor {
public:
    /// Resolve a glob pattern to matching filesystem paths.
    ParseResult parse(const std::vector<FunctorInput>& inputs,
                    const std::vector<FunctorOutput>& outputs,
                    const FunctorContext& context) override;
    /// Construct a glob pattern from context values.
    FormatResult format(const std::vector<FunctorInput>& inputs,
                      const FunctorContext& context) override;

private:
    /// Session-scoped cache: key = "basePath\0pattern" → glob results.
    std::unordered_map<std::string, std::vector<std::string>> m_globCache;

    /// Execute glob and cache results for the session.
    std::vector<std::string> cachedGlobFiles(const std::string& basePath,
                                              const std::string& pattern);
};

} // namespace functors
} // namespace silex
