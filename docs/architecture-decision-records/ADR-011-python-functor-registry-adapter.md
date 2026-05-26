# ADR-011: Python Functor Registry Adapter

**Status:** Accepted
**Date:** 2026-05-26
**Deciders:** Alex Telford
**Related:** ADR-002, ADR-007, ADR-009, ADR-010

## Context

Silex schemas can register Python functors that parse and format template values. We tried to register a custom ShotGrid-backed functor for asset variant parsing, but it was not being registered correctly through the C++ registry binding.

The Python functor object reached the binding layer, but the registry later invoked the C++ `IFunctor` interface without reliably dispatching back to the Python implementation. As a result, schema resolution missed the derived asset fields that the custom functor was responsible for providing.

The problem belonged at the Silex binding boundary. Fixing downstream path resolution would have hidden the registration issue and left any other Python functor exposed to the same behavior.

## Decision

Wrap Python functor objects in a C++ `IFunctor` adapter at registration time.

The adapter:

- stores a `py::object` to keep the Python functor alive for the registry factory lifetime
- implements `IFunctor::parse` and forwards to the Python object's `parse` method with the GIL held
- implements `IFunctor::format` and forwards to the Python object's `format` method with the GIL held
- converts C++ `FunctorInput` variants into a Python list before calling Python code
- returns the Python result cast back into the C++ `ParseResult` or `FormatResult`

The `Registry.register_functor` binding now accepts a Python object and registers a factory that returns the adapter. The registry therefore continues to operate on `std::shared_ptr<IFunctor>`, while Python-authored functors keep their Python implementation behavior when invoked through C++ resolver code.

## Summary of Changes

- added a `PythonFunctorAdapter` in the core pybind11 bindings
- changed `Registry.register_functor` to wrap Python functor objects in that adapter before registering the factory
- preserved the existing C++ registry contract of returning `std::shared_ptr<IFunctor>`
- forwarded both `parse` and `format` calls from C++ back to Python with the GIL held
- bumped the package version for the fixed wheels
- documented the Rez wheel-first development packaging workflow
- built and installed wheels for each supported Python ABI used in development validation

## Consequences

Python-authored functors now behave the same whether they are called directly from Python or indirectly through the C++ resolver registry. This keeps custom ShotGrid parsing logic in the schema functor layer instead of requiring downstream resolver fallbacks.

The binding now owns a Python object for each registered Python functor adapter, and each delegated call crosses the C++/Python boundary with the GIL held. That is the expected cost of supporting Python functors through the C++ registry.

Validation must include a registry-driven Python functor smoke test, not just direct Python method calls.

## Related Decisions

- ADR-002 defines the expression graph and functor extension model.
- ADR-007 records the C++17 and pybind11 port that introduced this binding boundary.
- ADR-009 defines parse and format terminology for schema and functor operations.
- ADR-010 defines the PyPI wheel packaging workflow used to validate this fix across Python versions.

## References

- `source/silex_core/bindings/bind_core.cpp`
- `pyproject.toml`
- `docs/build_and_test.md`
