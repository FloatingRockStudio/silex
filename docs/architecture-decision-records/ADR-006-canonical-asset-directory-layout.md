# ADR-006: Canonical Asset Directory Layout

**Status:** Accepted  
**Date:** 2026-03-18  
**Deciders:** alex.telford  
**Related:** ADR-003, ADR-005  

## Context

New projects are moving to a filesystem asset layout that separates the asset root from the asset variant root.

Previously, asset publishes were commonly stored as either:

- canonical flat asset roots: `Assets/<assetVariant>/...`
- legacy classified roots: `Assets/<classification>/<assetVariant>/...`

That no longer matches the desired structure for new projects.

The new layout should be:

- `Assets/<asset>/<assetVariant>/...`

At the same time:

- short contexts should stay unchanged as `/project/assets/<assetVariant>`
- Sceptre shot paths should stay unchanged
- existing asset publishes under classified roots must remain readable
- `fr_sceptre` and `fr_asset_api` should continue passing minimal context, primarily `asset_variant`

## Decision

Silex will treat `asset/assetVariant` as the canonical asset filesystem layout for new writes.

### 1. Keep short asset contexts unchanged

Short asset contexts remain:

- `/PROJECT/assets/<assetVariant>`

No extra `asset` or `classification` segment is introduced in short-context schemas.

### 2. Canonicalize filesystem asset roots to `asset/assetVariant`

Filesystem schemas for assets now resolve new writes to:

- `Assets/<asset>/<assetVariant>/...`

Examples:

- `Assets/chrAlex/chrAlexBase/...`
- `Assets/env/envBase/...`

### 3. Keep classified asset roots as deprecated read paths

Legacy filesystem paths remain modeled in schema as deprecated branches:

- `Assets/<classification>/<assetVariant>/...`

Deprecated branches are available for reads and migration workflows, but canonical writes must avoid them by default.

### 4. Preserve minimal consumer contracts

`fr_sceptre` and `fr_asset_api` should not branch on canonical vs deprecated filesystem layout.

They continue to:

- pass in minimal context
- center asset resolution around `asset_variant`
- rely on Silex-backed inference for additional asset components when needed for filesystem writes

### 5. Use nested shot asset-instance folders

Shot asset-instance filesystem paths now use:

- `Entity/<source_asset>/<instance_name>/...`

Example:

- `Entity/chrAlexBase/chrAlexBase_01/...`

This keeps the full instance identity while also grouping instances under their source asset.

## Rationale

### Why separate `asset` from `assetVariant`?

- groups related variants under a stable asset root
- matches the new project structure requirement
- makes asset browsing and migration clearer
- avoids reusing classification as a filesystem grouping rule for new work

### Why keep short contexts unchanged?

- short contexts are a user-facing convenience contract
- adding extra path segments would increase friction without adding useful context for callers
- filesystem hierarchy and short-context hierarchy do not need to be identical

### Why keep deprecated classified reads?

- existing publishes still need to resolve
- migration can happen incrementally
- ADR-005 already defines the lifecycle-aware traversal model needed for this

### Why keep consumers minimal?

- adapters should not need branch-specific filesystem knowledge
- canonical-vs-deprecated choice belongs in schema traversal, not in callers
- callers only care about stable context values and endpoints

## Consequences

### Positive

- new asset publishes land in the new project structure
- existing classified asset publishes remain discoverable
- short-context workflows remain stable
- shot asset instances are grouped more clearly by source asset

### Negative

- filesystem schemas become more complex because the same logical endpoint is reachable through canonical and deprecated branches
- some tests and fixtures need to move from flat asset roots to nested asset roots

### Mitigations

- use deprecated traversal instead of ad-hoc fallback logic
- keep consumer-side code focused on context normalization only
- cover canonical writes and deprecated reads separately in tests

## Related Decisions

- ADR-003: URI Support for fr_asset_api
- ADR-005: Deprecated Segment Traversal and Plural Resolution

## References

- [silex/search_path/fr_fs_projects.silex](../../search_path/fr_fs_projects.silex)
- [fr_sceptre/silex_schemas/sceptre_context.silex](../../../fr_sceptre/silex_schemas/sceptre_context.silex)
- [fr_sceptre/silex_schemas/sceptre_fs.silex](../../../fr_sceptre/silex_schemas/sceptre_fs.silex)
- [fr_asset_api/silex_schemas/fra_fs_asset.silex](../../../fr_asset_api/silex_schemas/fra_fs_asset.silex)

---

**© 2026 Floating Rock Studios. All rights reserved.**
