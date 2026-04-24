# ADR-005: Deprecated Segment Traversal and Plural Resolution

**Status:** Accepted  
**Date:** 2026-03-13  
**Deciders:** alex.telford  
**Related:** ADR-001, ADR-004  

## Context

Silex schemas need to retire legacy path layouts without breaking reads against files that already exist on disk.

Before this change, a schema path was effectively singular:

- endpoints were indexed as one path per endpoint uid
- resolution stopped at the first successful full match
- results did not indicate whether a match came from a legacy route
- deprecating a path required removing it outright or keeping it fully active

That made migration difficult. Publishing should prefer the current canonical layout, while read and query operations still need the option to inspect deprecated layouts.

## Decision

Silex will treat deprecated path segments as optional legacy traversal nodes.

### 1. Add lifecycle metadata to segments

Schemas may mark segments, templates, and overrides with `is_deprecated`.

During schema loading this is mapped onto `SegmentFlags.DEPRECATED` so traversal logic can reason about deprecation without re-reading raw schema data.

### 2. Skip deprecated segments by default

When `include_deprecated` is not requested:

- a deprecated segment is treated as if it does not exist in the canonical path
- its child segments are traversed from the deprecated segment's parent
- path generation prefers the non-deprecated route
- endpoint filtering does not surface deprecated-expanded endpoint paths

This keeps new writes on the canonical path while still allowing a schema to model legacy structure.

### 3. Expand traversal when deprecated paths are explicitly requested

When `include_deprecated=True`:

- resolvers first consider the canonical path with deprecated segments skipped
- resolvers also consider the legacy path with deprecated segments included
- read and write operations return every successful match instead of stopping at the first one

This allows callers to query both current and legacy layouts in a single operation.

### 4. Preserve multiple endpoint paths per endpoint uid

Endpoint indexing will store all schema paths for an endpoint uid, not only the last discovered path.

This is required for:

- switch branches that lead to the same endpoint uid
- canonical and deprecated-expanded endpoint paths
- endpoint-path filtering using dotted endpoint-path patterns
- globbed endpoint-path filters that can target full paths or dotted subpaths such as `tree=shots.*.metadata`

### 5. Report deprecation on each match

Resolve results keep compatibility fields for legacy callers, but also expose plural match entries.

Each match records:

- the endpoint uid
- the concrete endpoint path used for the match
- whether deprecated traversal participated in producing that match

Callers can therefore exclude deprecated matches for publishing decisions while still inspecting them during search and migration workflows.

## Rationale

This approach separates two concerns cleanly:

- **authoring canonical paths** for new data
- **querying legacy paths** for existing data

The chosen model keeps deprecation in the schema rather than in ad-hoc resolver rules.

Alternative approaches considered:

### Remove deprecated paths entirely

- **Pros:** simpler traversal model
- **Cons:** breaks reads for existing files and forces hard cutovers

### Keep deprecated paths fully active

- **Pros:** no migration work in resolver
- **Cons:** new writes continue targeting legacy layouts and endpoint queries remain ambiguous without visibility

### Lifecycle-aware traversal with plural matches (chosen)

- **Pros:** canonical writes by default, optional legacy reads, explicit ambiguity reporting
- **Cons:** more candidate expansion and more result handling for callers

## Consequences

### Positive

- New writes avoid deprecated routes by default
- Existing files on deprecated layouts remain queryable when requested
- Endpoint-path filters can target specific schema routes
- Partial globbed endpoint-path filters can target branches without repeating the full schema root
- Ambiguous endpoint definitions are preserved instead of overwritten
- Match results carry enough metadata for migration tooling

### Negative

- Resolver behavior is more complex because one schema path can expand into several candidates
- Callers that care about ambiguity should inspect plural matches instead of singular compatibility fields
- Including deprecated routes increases candidate count and may increase resolve cost

### Mitigations

- Keep singular compatibility fields populated from the preferred non-deprecated match
- Only expand deprecated candidates when explicitly requested
- Preserve stable ordering of endpoint candidates and matches
- Cover traversal and endpoint indexing rules with unit tests

## Related Decisions

- ADR-001: Graph-Based Schema Architecture
- ADR-004: Context Mutation During Path Writing

## References

- `python/silex/_internal/schema_loader.py`
- `python/silex/_internal/schema_utils.py`
- `python/silex/impl/resolvers/generic.py`
- `python/silex/structs.py`
- `schema/silex_schema.json`
- `tests/unit/test_deprecated_traversal.py`
- `tests/unit/test_multi_path_resolution.py`
- `tests/unit/test_resolve_structs.py`

---

**© 2026 Floating Rock Studios. All rights reserved.**
