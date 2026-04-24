# Troubleshooting

## `import silex` Fails

Common causes:

- the package was not built successfully
- runtime DLLs were not present next to the Python extension

Rebuild with `python -m build --wheel` and verify the installed wheel or local package directory contains the extension module and dependent DLLs.

## No Schemas Are Found

Silex only discovers `.silex` files from directories on `SILEX_SCHEMA_PATH`.

Check:

- the environment variable is set
- the schema directory contains `*.silex` files
- your configured schema roots are included in `SILEX_SCHEMA_PATH`

## A Path Matches The Wrong Schema

Review:

- `pattern`
- `root_path`
- `context_filters`

Keep schema `pattern` values broad enough to admit valid inputs but narrow enough to exclude unrelated trees. If multiple schemas overlap, tighten the pattern or the context filters instead of relying on incidental ordering.

## `path_from_context(...)` Returns Missing Context

This usually means the target endpoint requires keys that were never populated.

Check:

- `write` expressions on the traversed segments
- `write_update_keys`
- endpoint selection
- whether the branch is `is_readonly`

## Deprecated Traversal Appears In Results

If `used_deprecated_traversal` is true, the resolver found a match through a deprecated branch.

That is usually a schema migration signal. Prefer documenting the replacement endpoint and updating callers rather than hiding the deprecated path in code.

## URI Query Parameters Behave Unexpectedly

For URI schemas, inspect:

- the URI segmenter in use
- `partition_segmenter`
- `partitions`
- fallback partitions marked with `is_fallback`

Unexpected matches often come from overly broad fallback partitions or from partition ordering that does not match the serialized URI form.