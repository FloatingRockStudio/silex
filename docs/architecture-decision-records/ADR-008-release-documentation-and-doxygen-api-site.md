# ADR-008: Release Documentation and Doxygen API Site

**Status:** Accepted
**Date:** 2026-04-17
**Deciders:** alex.telford
**Related:** ADR-007

## Context

Silex is being prepared for release as a reusable package rather than an internal code drop. The repository already had useful conceptual documentation, but it lacked a consistent release-facing documentation system:

- there was no packaged API reference generated from the shipped public surface
- user-facing guides and engineering material were not clearly separated for release consumers
- documentation output was not part of the package build, so drift between code and docs would increase over time

An earlier stop-gap approach generated Markdown reference pages with a custom script. That reduced the immediate gap, but it duplicated functionality that a standard documentation tool already provides and would require ongoing maintenance.

At the same time, the release build already vendors its Python packaging dependencies and can resolve Doxygen in CI, which keeps documentation generation repeatable without relying on a separate package manager workflow.

## Decision

Adopt Doxygen as the standard release documentation generator for Silex and run it automatically during the standard build and release flow.

### Specific Decisions

**1. Keep authored documentation in Markdown under `docs/`**

The existing manual documentation remains the source of truth for guides, architecture notes, schema authoring guidance, troubleshooting, and release-facing entry points.

**2. Generate packaged HTML documentation during the build**

The build invokes Doxygen using a checked-in `docs/Doxyfile.in` template. Generated output is written into `build/docs/html` and installed with the package or published via CI.

**3. Resolve Doxygen through the standard build environment and CI**

The documentation toolchain is part of the same reproducible build and CI environment as CMake, Python, and test dependencies.

**4. Document both the public C++ surface and the bound interface sources**

The Doxygen input includes:

- `docs/`
- `source/silex_core/include/silex/`
- `source/silex_core/bindings/`

This gives one packaged site that covers release guides, public headers, and the binding implementation surface.

## Rationale

### Why Doxygen?

- it is an established documentation system for C++ APIs
- it can consume Markdown and source code in one pass
- it produces a professional browsable HTML site with indexes, class pages, file pages, and cross-links
- it removes the need to maintain a custom extraction script for generated reference output
- it fits naturally into the existing CMake and CI-based build workflow

### Why keep Markdown guides instead of moving everything into code comments?

API reference and user guidance solve different problems. Path resolution behavior, schema authoring patterns, and release workflows are better expressed in authored Markdown than in header comments alone. The chosen approach keeps narrative guidance in Markdown and lets Doxygen render it into the same packaged site as the API material.

### Alternatives Considered

1. **Custom documentation generator script**
   Rejected because it duplicates capabilities already available in Doxygen and increases maintenance cost.

2. **Manual docs only, no generated API output**
   Rejected because it would drift too easily from the shipped headers and binding layer.

3. **Separate documentation pipeline outside the package build**
   Rejected because release artifacts should contain the generated documentation they were built from.

## Consequences

### Positive

- release builds now produce packaged HTML documentation automatically
- Doxygen is resolved reproducibly through the normal build and CI flow
- manual docs and generated API material are shipped together
- the generated site reflects the public headers and binding sources in the same build that produces the package

### Negative

- documentation quality now depends more directly on the quality of Doxygen comments and source annotations
- binding-source pages are useful, but they are not a perfect substitute for a fully curated Python API reference
- Doxygen configuration becomes part of the maintained build surface

### Mitigations

- improve public header comments over time, especially for high-value release symbols
- continue expanding user-facing Markdown guides for workflows that API pages do not explain well
- document the generated site entry point in the README and build documentation

## Related Decisions

- [ADR-007: C++ Port and Architecture Redesign](ADR-007-cpp-port-and-architecture-redesign.md)

## References

- [docs/index.md](../index.md)
- [docs/build_and_test.md](../build_and_test.md)
- [docs/Doxyfile.in](../Doxyfile.in)

---

**© 2026 Floating Rock Studios. All rights reserved.**