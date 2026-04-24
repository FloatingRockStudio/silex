# ADR-010: PyPI Packaging and GitHub Actions Release Pipeline

**Status:** Accepted
**Date:** 2026-04-23
**Deciders:** alex.telford
**Related:** ADR-007, ADR-008, ADR-009

## Context

Silex was being prepared for open-source release, but the repository was still optimized around an internal packaging workflow:

- the native build was not exposed through standard Python packaging entrypoints
- there was no PyPI-ready wheel or source distribution configuration
- release automation for platform wheels, PyPI publication, and public documentation deployment was missing
- the repository contained packaging-specific compatibility surfaces that were not part of the intended public workflow

For open-source consumers, the primary installation path needs to be `pip install silex`. At the same time, the existing C++17 and pybind11 build graph should remain authoritative rather than being duplicated in a second build system.

## Decision

Adopt a PyPI-first packaging strategy built on top of the existing CMake project as the supported release workflow.

### Specific Decisions

**1. Use `scikit-build-core` as the Python build backend**

`pyproject.toml` becomes the canonical packaging entrypoint. Python wheels and source distributions are built through standard PEP 517 tooling, while CMake remains the source of truth for the compiled extension.

**2. Keep the current C++ sources in place and package them through CMake**

The repository does not duplicate or rewrite the native build in setuptools. Instead, the existing targets are adjusted so wheel builds install the compiled extension and required runtime artifacts in an importable layout.

**3. Remove the legacy packaging surface from the supported workflow**

The repository no longer relies on a separate internal package recipe or helper script for its public build path. Python packaging is driven exclusively through `pyproject.toml` and the standard PEP 517 frontend.

**4. Build platform wheels with GitHub Actions and `cibuildwheel`**

GitHub Actions is the authoritative public release pipeline. The repository builds wheels for Windows, Linux, and macOS, produces a source distribution, and publishes tagged releases to PyPI through trusted publishing.

**5. Publish release documentation from GitHub Actions**

The generated Doxygen site is built in CI and deployed to GitHub Pages so the public documentation surface matches the packaged release surface.

## Rationale

### Why `scikit-build-core`?

- it is designed specifically for Python packaging of CMake-based native extensions
- it avoids reimplementing the native build graph in a second tool
- it supports modern PEP 517 builds cleanly
- it works naturally with `cibuildwheel`

### Why remove the legacy packaging files from the supported path?

Once the public package and CI flow were validated through `pyproject.toml`, the remaining legacy packaging files stopped contributing to the supported release path and only added extra documentation and maintenance surface.

### Why GitHub Actions for releases?

- wheels must be built per platform and Python version
- PyPI trusted publishing integrates cleanly with GitHub-hosted CI
- the same system can publish both binaries and documentation

### Alternatives Considered

1. **Keep a non-standard internal packaging path as the official build route**
   Rejected because it does not give open-source users a standard `pip install` workflow.

2. **Rewrite the build around setuptools-only custom commands**
   Rejected because it would duplicate CMake logic and increase long-term maintenance cost.

3. **Publish source-only releases and require local native compilation**
   Rejected because the intended audience needs prebuilt wheels for common platforms.

## Consequences

### Positive

- Silex can be built with standard Python packaging commands
- the repository is ready for PyPI publication with prebuilt wheels
- the public release path is automated for wheels, sdists, and docs
- the existing CMake project remains the authoritative native build description

### Negative

- fetched third-party dependencies still influence wheel contents and install behavior
- release automation adds more repository-maintained infrastructure

### Mitigations

- validate wheel importability as part of the packaging workflow
- continue trimming unnecessary native install artifacts as the OSS packaging path matures

## Related Decisions

- [ADR-007: C++ Port and Architecture Redesign](ADR-007-cpp-port-and-architecture-redesign.md)
- [ADR-008: Release Documentation and Doxygen API Site](ADR-008-release-documentation-and-doxygen-api-site.md)
- [ADR-009: Parse/Format Terminology For Schema And Functor Operations](ADR-009-parse-format-terminology-for-schema-and-functor-operations.md)

## References

- [pyproject.toml](../../pyproject.toml)
- [setup.py](../../setup.py)
- [ci.yml](../../.github/workflows/ci.yml)
- [publish.yml](../../.github/workflows/publish.yml)
- [docs.yml](../../.github/workflows/docs.yml)
- [docs/build_and_test.md](../build_and_test.md)

---

**© 2026 Floating Rock Studios. All rights reserved.**