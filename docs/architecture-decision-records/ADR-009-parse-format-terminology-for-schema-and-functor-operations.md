# ADR-009: Parse/Format Terminology For Schema And Functor Operations

**Status:** Accepted
**Date:** 2026-04-23
**Deciders:** alex.telford
**Related:** ADR-002, ADR-004, ADR-007

## Context

Silex historically described its two directional operations as `read` and `write`.

That terminology was serviceable when the system was mostly discussed as a filesystem path resolver, but it became increasingly misleading as the public API matured:

- schemas are used to transform both filesystem paths and URIs, not just files on disk
- functors do not literally perform I/O; they transform values in one direction or the other
- templates and expression evaluation are value transformations rather than storage operations
- the C++ and pybind11 public APIs were being stabilized for open-source release, so the naming needed to be precise and portable

The older vocabulary also leaked into multiple layers at once: schema JSON keys, bound Python attributes, functor method names, and internal helper naming. That made it harder to explain the system consistently and harder to reason about API behavior outside a file-centric mental model.

## Decision

Adopt `parse` and `format` as the canonical directional terminology for Silex schemas, functors, and public bindings.

### Specific Decisions

**1. Replace schema-level `read`/`write` with `parse`/`format`**

Schema definitions now use `parse` for input-to-context extraction and `format` for context-to-output generation.

**2. Align bound segment fields and related metadata with the new terms**

The public `SilexSegment` binding exposes `parse`, `format`, and `format_update_keys`. Public template-related naming stays aligned through `template` while the underlying C++ field remains `templateName`.

**3. Align functor concepts with value transformation rather than I/O**

Functor-facing types and results use `parse` and `format` terminology so their role is explicit: they transform values in one direction or the other, not perform reads or writes against storage.

**4. Keep the directional semantics unchanged**

This is a terminology and API-surface clarification, not a behavioral redesign. The underlying resolver traversal model, context mutation behavior, and template application rules remain the same.

## Rationale

### Why `parse` and `format`?

- they describe transformation direction without implying storage I/O
- they apply equally well to filesystem paths, URI segments, query partitions, and templates
- they align more naturally with functor semantics and expression processing
- they reduce ambiguity when documenting the Python and C++ APIs for public use

### Why make this change across the public surface instead of aliasing forever?

The public API was already being tightened during the C++/pybind11 transition. Preserving both vocabularies indefinitely would keep documentation and tests in a mixed state and make the OSS surface harder to teach. A single canonical terminology is easier to document and maintain.

### Alternatives Considered

1. **Keep `read` and `write` everywhere**
   Rejected because it over-emphasizes file I/O and does not fit template or functor value transformation well.

2. **Support both vocabularies indefinitely as equal aliases**
   Rejected because it would preserve ambiguity in schemas, tests, and documentation with little long-term benefit.

3. **Use `decode` and `encode` instead**
   Rejected because those terms fit serialization better than general path and URI transformation.

## Consequences

### Positive

- schema terminology is clearer and more consistent across filesystem, URI, and template use cases
- the bound Python API reads more naturally for value transformation operations
- functor and resolver documentation can use one vocabulary throughout

### Negative

- tests, fixtures, and documentation that still used `read`/`write` needed to be updated
- older internal or experimental material may still mention the legacy terms historically

### Mitigations

- update the JSON schema and public bindings together so the public surface stays coherent
- update test fixtures and validation expectations alongside the rename
- document the new canonical terminology in the API and architecture material

## Related Decisions

- [ADR-002: Expression Graph System with Functors](ADR-002-expression-graph-with-functors.md)
- [ADR-004: Context Mutation During Path Writing](ADR-004-context-mutation-during-write.md)
- [ADR-007: C++ Port and Architecture Redesign](ADR-007-cpp-port-and-architecture-redesign.md)

## References

- [schema/silex_schema.json](../../schema/silex_schema.json)
- [source/silex_core/bindings/bind_structs.cpp](../../source/silex_core/bindings/bind_structs.cpp)
- [docs/api_guide.md](../api_guide.md)

---

**© 2026 Floating Rock Studios. All rights reserved.**