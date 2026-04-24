#pragma once

/// @file BuiltinRegistrar.h
/// @brief Registers all built-in functors and segmenters into a Registry.

namespace silex {
namespace core {

class Registry;

/// Register all built-in C++ functors and segmenters into the registry.
void registerBuiltins(Registry& registry);

} // namespace core
} // namespace silex
