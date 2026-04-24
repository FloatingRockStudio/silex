# Architecture Decision Records (ADRs)

This directory contains Architecture Decision Records for Silex and fr_asset_api.

## What is an ADR?

An Architecture Decision Record (ADR) captures an important architectural decision made along with its context and consequences. ADRs help teams understand why systems are designed the way they are.

## Silex ADRs

### Core Architecture

- [ADR-001: Graph-Based Schema Architecture](ADR-001-graph-based-schema-architecture.md)
  - Decision to model path structure as directed acyclic graph
  - Shift from linear sequential processing to graph traversal
  - **Status**: Accepted | **Date**: 2025-08-19

- [ADR-002: Expression Graph System with Functors](ADR-002-expression-graph-with-functors.md)
  - Expression dependency graphs with topological sorting
  - Plugin-based functor system for path transformations
  - **Status**: Accepted | **Date**: 2025-08-25

### Integration & Features

- [ADR-003: URI Support for fr_asset_api](ADR-003-uri-support-for-fr-asset-api.md)
  - URI-based asset addressing with `fra://` scheme
  - Integration with Silex URI segmenter
  - **Status**: Accepted | **Date**: 2025-09-01

- [ADR-004: Context Mutation During Path Writing](ADR-004-context-mutation-during-write.md)
  - Allowing context updates during path generation
  - Trade-offs between mutability and API simplicity
  - **Status**: Accepted | **Date**: 2025-08-29

- [ADR-005: Deprecated Segment Traversal and Plural Resolution](ADR-005-deprecated-segment-traversal-and-plural-resolution.md)
  - Lifecycle-aware deprecated segment traversal for canonical vs legacy paths
  - Multi-path endpoint indexing and plural resolve match reporting
  - **Status**: Accepted | **Date**: 2026-03-13

- [ADR-006: Canonical Asset Directory Layout](ADR-006-canonical-asset-directory-layout.md)
  - Canonical asset filesystem roots move to `asset/assetVariant`
  - Classified asset roots remain deprecated read-only branches
  - **Status**: Accepted | **Date**: 2026-03-18

### Implementation

- [ADR-007: C++ Port and Architecture Redesign](ADR-007-cpp-port-and-architecture-redesign.md)
  - Full C++17 port with pybind11 bindings replacing pure Python implementation
  - Dependency reduction, PIMPL public boundary, domain-grouped source layout
  - **Status**: Accepted | **Date**: 2026-05-14

- [ADR-008: Release Documentation and Doxygen API Site](ADR-008-release-documentation-and-doxygen-api-site.md)
  - Release documentation packaged with the build
  - Doxygen-generated HTML site from docs, headers, and binding sources
  - **Status**: Accepted | **Date**: 2026-04-17

- [ADR-009: Parse/Format Terminology For Schema And Functor Operations](ADR-009-parse-format-terminology-for-schema-and-functor-operations.md)
  - Canonical rename from `read`/`write` to `parse`/`format`
  - Public schema, functor, and binding terminology aligned around value transformation
  - **Status**: Accepted | **Date**: 2026-04-23

- [ADR-010: PyPI Packaging and GitHub Actions Release Pipeline](ADR-010-pypi-packaging-and-github-actions-release-pipeline.md)
  - PyPI-first packaging built on the existing CMake project
  - GitHub Actions pipelines for wheels, releases, and public docs
  - **Status**: Accepted | **Date**: 2026-04-23

## ADR Format

Each ADR follows this structure:

```markdown
# ADR-NNN: Title

**Status:** [Proposed | Accepted | Deprecated | Superseded]
**Date:** YYYY-MM-DD
**Deciders:** Names
**Related:** Other ADRs, tickets

## Context
The issue or problem driving this decision

## Decision
The change being proposed or implemented

## Rationale
Why this decision was made, alternatives considered

## Consequences
Positive and negative outcomes, mitigations

## Related Decisions
Links to other ADRs

## References
Code, commits, documentation references
```

## When to Create an ADR

Create an ADR when making decisions that:

- Affect system architecture or design
- Have long-term implications
- Involve significant trade-offs
- May be questioned later
- Set precedents for future work

## ADR Workflow

1. **Propose**: Create ADR with status "Proposed"
2. **Discuss**: Review with team
3. **Decide**: Update status to "Accepted" or "Rejected"
4. **Implement**: Reference ADR in code/commits
5. **Update**: If decision changes, create superseding ADR

## Related Documentation

- [Silex API Guide](../api_guide.md) - Current Python and C++ API usage
- [Silex Architecture](../architecture.md) - Current implementation details and diagrams

---

**© 2025 Floating Rock Studios. All rights reserved.**
