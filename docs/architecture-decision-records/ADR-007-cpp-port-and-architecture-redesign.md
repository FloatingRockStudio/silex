# ADR-007: C++ Port and Architecture Redesign

**Status:** Accepted
**Date:** 2026-05-14
**Deciders:** alex.telford
**Related:** ADR-001, ADR-002, PIPE-124

## Context

The Silex path resolver was originally implemented in pure Python. As the system matured and became a critical path in production tooling, several limitations emerged:

- **Performance**: Python's overhead was measurable when resolving thousands of paths per session (scene load, batch publish, validation passes)
- **Dependency weight**: The Python implementation pulled in `networkx` (DAG operations), `pyjson5` (schema parsing), and `jsonschema` (schema validation) — heavy dependencies for what amounted to targeted functionality
- **API surface leakage**: Internal implementation modules (prefixed with `_internal/`) were imported by consumers, creating coupling to implementation details

## Decision

Port the entire Silex core to C++17 with pybind11 Python bindings. Simultaneously redesign the source architecture to enforce proper public/private boundaries and reduce dependency count.

### Key Design Choices

**1. Single shared library (`silex_core`) with pybind11 bindings (`_silex_core`)**

All C++ code compiles into one shared library. Python accesses it through a thin pybind11 binding layer that converts between Python dicts/lists and C++ `std::map<std::string, std::any>` / `std::vector`. This eliminates the need for separate Python packages for internals.

**2. Public headers under `include/silex/`, private implementation under `src/`**

Only the resolver interface, data structures, constants, and abstract interfaces are public. All implementation details (registry, schema loading, expression parsing, evaluation, functors, segmenters) are private to the library. Consumers cannot `#include` private headers.

**3. PIMPL on GenericResolver only**

The public-facing `GenericResolver` uses PIMPL to hide its dependency on internal components (`Registry`, `FileSchemaLoader`, `ExpressionParser`, `ExpressionEvaluator`). Internal classes that are never exposed publicly do not use PIMPL — their headers are private to `src/` and the added indirection would serve no purpose.

**4. Replace Python dependencies with C++ equivalents**

| Python Dependency | Replacement | Rationale |
|-------------------|-------------|-----------|
| `networkx` | `internal::DAG` (~50 lines) | Only used for topological sort; a full graph library is unnecessary |
| `pyjson5` | `json5cpp` (FetchContent) | Lightweight JSON5 parser, fetched at build time |
| `jsonschema` | `SchemaValidator` + `nlohmann_json` | Manual validation against the Silex JSON Schema definition |
| `python_box` | `std::map<std::string, std::any>` | Dot-notation access reimplemented in `Utils.h` |

**5. Keyed singleton Registry**

The Registry uses a keyed singleton pattern (`Registry::instance("key")`) rather than dependency injection. This matches how production schemas work: a single registry instance holds all registered functors, segmenters, and schemas for a resolver session. The key allows multiple isolated registries when needed (e.g., testing).

**6. Domain-grouped source layout**

Internal source is organized by domain rather than by layer:

```
src/
├── registry/       # Component storage and factory management
├── schema/         # .silex file discovery, parsing, validation
├── expression/     # Expression string parsing and DAG evaluation
├── resolver/       # GenericResolver implementation + helpers
├── functors/       # Built-in functor implementations
├── segmenters/     # Filesystem and URI segmenters
└── util/           # Logging, string utilities
```

**7. SILEX_API export macro with WINDOWS_EXPORT_ALL_SYMBOLS fallback**

Public symbols use explicit `SILEX_API` (`__declspec(dllexport/dllimport)`) markup. `WINDOWS_EXPORT_ALL_SYMBOLS` remains enabled temporarily so that test executables can link against internal symbols without exporting them explicitly. A future change will use CMake OBJECT libraries to give tests direct access to internal code.

**8. FetchContent dependency chain: rez -> bundled -> fetch**

CMake first checks if dependencies are available via rez packages, then falls back to bundled sources, then fetches from Git. This supports both CI/CD (rez) and standalone developer builds.

## Rationale

### Why C++?

- **10–50× faster** path resolution in benchmarks (regex matching, string operations, map lookups)
- **Zero Python import cost**: The compiled `.pyd` loads instantly vs. importing dozens of Python modules
- **Single artifact**: One DLL/SO per platform, no `pip install` conflicts

### Why not keep a parallel Python implementation?

Maintaining both languages doubles the test surface and creates drift risk. The pybind11 bindings provide a Pythonic API (`snake_case` methods, dict returns) that is indistinguishable from native Python to consumers. The Python package (`python/silex/`) retains only the public-facing types (`interfaces.py`, `structs.py`, `constants.py`) for type checking and IDE support.

### Alternatives Considered

1. **Cython**: Stop-gap measure at best, when scaling to larger asset systems this would result in similar issues.
2. **Rust with PyO3**: Limited integration support and current developer team not comfortable supporting rust codebases at this time.
3. **Keep Python, optimize hot paths**: Profile-guided optimization of the Python code showed that the overhead was fundamental (dict lookups, regex compilation, import time), not fixable with micro-optimizations

## Consequences

### Positive

- **Performance**: Sub-millisecond resolution for common paths (was 5–20ms in Python)
- **Reduced dependencies**: `requires` list dropped from 5 packages to 2 (`python`, `python_box`)
- **Clean API boundary**: Consumers can only use `GenericResolver`, data structs, and constants — no internal access
- **Portable**: Same binary works across all Python 3.9+ DCC environments on Windows
- **Testable**: C++ tests via GTest run without Python; Python tests via pytest exercise the bindings

### Negative

- **Build complexity**: CMake + FetchContent integration is more complex than `setup.py`
- **Debugging**: C++ stack traces are less readable than Python tracebacks for schema authors
- **Development velocity**: C++ iteration is slower than Python for schema/functor development
- **Type erasure**: `std::any` maps lose Python's dynamic typing flexibility (e.g., no arbitrary nesting without explicit `any_cast`)
- **Pip Deployment**: Future deployment via pypi will become more complicated than with pure python.

### Mitigations

- Detailed Mermaid diagrams and architecture docs reduce onboarding friction
- Verbosity levels (Quiet -> Trace) provide schema-author-friendly debugging without C++ knowledge
- Custom functors can still be written in Python and registered via the binding layer (future work)
- Expression caching amortizes parse cost across repeated resolutions

## References

- Source restructuring: this session
- Build system: [CMakeLists.txt](../../CMakeLists.txt)
- Architecture docs: [architecture.md](../architecture.md)
- API guide: [api_guide.md](../api_guide.md)

---

**© 2026 Floating Rock Studios. All rights reserved.**
