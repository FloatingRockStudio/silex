# Writing .silex Files

This guide covers how to author `.silex` schema files for the current Silex implementation.

It is written against the C++ schema loader and resolver in `source/silex_core`, using the validation schema in `schema/silex_schema.json` and the working examples under `examples/` and `tests/resources/`.

## What a .silex File Does

A `.silex` file describes how Silex should:

- recognize a family of paths or URIs,
- split them into segments,
- parse values from those segments into structured context,
- format new paths from context,
- expose named endpoints that callers can resolve to.

In practice, a schema is a tree of named segments rooted under a `root_path`, plus reusable templates, aliases, and configuration.

## File Format

`.silex` files are parsed as JSON5, not strict JSON.

That means you can use:

- comments,
- trailing commas,
- unquoted object keys only when JSON5 allows them,
- multi-line formatting that would be rejected by strict JSON.

The loader scans every directory in `SILEX_SCHEMA_PATH` for `*.silex` files.

## Authoring Workflow

The safest authoring loop is:

1. Start from a minimal working schema.
2. Add one branch or endpoint at a time.
3. Verify both parse and format behavior.
4. Add templates only after the raw segment behavior is correct.
5. Add imports or inheritance only when the standalone version is stable.

For validation, use both structural checks and resolver behavior checks:

- structural validation against `schema/silex_schema.json`,
- `context_from_path(...)` tests for path-to-context parsing,
- `path_from_context(...)` tests for context-to-path formatting,
- endpoint-specific tests for the exact traversal you expect.

## Minimal Schema

This is the smallest practical filesystem schema:

```json5
{
    "uid": "example.basic",
    "pattern": ".*example_project.*",
    "root_path": "./data",
    "segmenter": "silex.FilesystemSegmenter",
    "context_filters": [
        {"project.code": "example_project"}
    ],
    "paths": {
        "project": {
            "pattern": "example_project",
            "parse": {
                "targets": {
                    "project.code": {"group": 0},
                    "context.project": {"group": 0}
                }
            },
            "format": "example_project",
            "endpoint": "context.project"
        }
    }
}
```

Even this minimal file already shows the main moving parts:

- schema identity via `uid`,
- schema selection via `pattern` and `context_filters`,
- traversal root via `root_path`,
- segment splitting via `segmenter`,
- the segment tree under `paths`,
- parse and format rules on a segment,
- an `endpoint` callers can target.

## Top-Level Fields

The following fields matter most in real schemas.

| Field | Purpose | Notes |
|------|---------|-------|
| `uid` | Unique schema identifier | Required for resolvable schemas. Use stable dotted names. |
| `import` | Bring in templates, aliases, config, and component registrations | In the current loader, use an array form such as `["./common", "./silex_core"]`. Relative imports are resolved from the current file. Non-relative imports are searched on `SILEX_SCHEMA_PATH`. |
| `extends` or `inherits` | Inherit another schema's document and segment tree | Use a single string. The current loader merges one base schema, not a list of parents. |
| `pattern` | Regex used to quickly match candidate paths | Applied before deep traversal. Keep it broad enough to admit valid paths and narrow enough to exclude unrelated trees. |
| `root_path` | Base path or URI root for this schema | Environment variables are expanded. Relative filesystem paths are resolved relative to the schema file location. |
| `segmenter` | Segmenter UID or name | Filesystem schemas usually use `silex.FilesystemSegmenter`. |
| `context_filters` | Fast schema selection from existing context | The array is OR logic. Each object inside it is AND logic across its keys. |
| `functors` | Additional functor registrations | Usually only needed for external or project-specific functors. |
| `segmenters` | Additional segmenter registrations | Usually only needed for custom URI or partition logic. |
| `aliases` | Short names for functors or segmenters | Commonly used to shorten `$silex.impl.functors.case_split.SplitCamelCaseFunctor(...)` into `$CC(...)`. |
| `templates` | Reusable segment definitions | Use these heavily for repeated version, frame, asset, or filename patterns. |
| `config` | Schema-level variables and matching behavior | Most useful for shared variables such as lexicons and for `case_sensitive`. |
| `paths` | Root segment tree | This is where most schema logic lives. |
| `overrides` | Patch inherited segments by dot path | Useful, but currently implementation-sensitive. Prefer simple pattern and lifecycle-flag overrides unless you have tests covering more advanced behavior. |

## Imports and Inheritance

`import` and `extends` solve different problems.

Use `import` when you want to share reusable definitions without making the imported file your base schema.

Imported content is currently merged for:

- `templates`,
- `aliases`,
- `config`,
- component registration through `functors` and `segmenters`.

Use `extends` or `inherits` when you want to start from another schema's full document and segment tree, then specialize it.

The current loader behavior is:

- `extends` and `inherits` are treated as synonyms,
- only one parent schema is merged,
- the child schema overlays the parent document,
- `overrides` are then applied to the built segment tree.

Recommended pattern:

- Put common templates, aliases, and config in imported utility schemas.
- Put reusable path trees in parent schemas.
- Keep inheritance shallow unless there is a strong reason to stack it.

## Segment Anatomy

Every entry under `paths` defines one named segment. A segment can represent a directory, file name, URI token, switch node, or dynamic partition point.

Example:

```json5
"version": {
    "pattern": "v([0-9]{3,4})",
    "target": "entity.version",
    "format": "$glob_tag(v%TAG%, {entity.version}, 3, entity.version)",
    "format_update_keys": ["entity.version"],
    "endpoint": "asset.version"
}
```

Common segment fields:

| Field | Purpose |
|------|---------|
| `pattern` | Regex used to match the incoming segment value |
| `target` | Shorthand for mapping the segment's raw `value` into one context key |
| `targets` | Explicit mapping from context keys to capture groups or expression outputs |
| `parse` | Expressions and mappings used when resolving path to context |
| `format` | Expressions used when generating a path from context |
| `endpoint` | Marks this segment as a named resolver endpoint |
| `template` | Applies a reusable template definition before local overrides |
| `paths` | Child segments for the default branch |
| `options` | Convenience list used to generate a regex alternation when no `pattern` is given |
| `switch` | Named branches chosen by the current segment value |
| `partitions` | Dynamic child groups under a segment |
| `partition_segmenter` | Segmenter used to split partition content |
| `ordered_partitions` | Whether partitions must be resolved in order |
| `format_update_keys` | Additional keys to prepopulate during format traversal |
| `placeholders` | Placeholder values attached directly to the segment |
| `is_deprecated` | Marks the segment deprecated |
| `is_readonly` | Allows parsing but suppresses formatting unless placeholders are in play |
| `is_intermediate` | Marks structural segments that should not usually be treated as meaningful endpoints |
| `is_fallback` | Marks a branch as fallback-only |

## Parse Rules

Parse rules control how a matched segment value becomes context.

### Target shorthand

If you only need to move the whole segment value into one key, use `target`:

```json5
"asset_name": {
    "pattern": "[a-zA-Z][a-zA-Z0-9_]*",
    "target": "context.asset",
    "format": "{context.asset}",
    "endpoint": "asset"
}
```

This is shorthand for mapping the raw segment `value` into a target key.

### Explicit targets

When you need capture groups or type conversion, use `targets`:

```json5
"parse": {
    "targets": {
        "context.sequence": {"group": 1},
        "context.shot": {"group": 2},
        "file.version": {"group": 3, "type": "int"}
    }
}
```

Useful target fields:

- `group`: capture group number,
- `type`: `string`, `int`, `float`, or `bool`,
- `is_array`: mark the target as array-like.

### Parse expressions

Use `parse.expressions` when capture groups alone are not enough:

```json5
"parse": {
    "expressions": [
        "$CC({value})->classification:0,asset:0--1",
        "$L({classification})->classification"
    ],
    "targets": {
        "context.classification": "classification",
        "context.asset": "asset"
    }
}
```

This lets you:

- split composite values,
- normalize case,
- expand aliases through lexicons,
- assign expression outputs into final context keys.

## Format Rules

Format rules control how context becomes a segment string.

### Simple literal format

```json5
"format": "Production"
```

### Context substitution

```json5
"format": "{context.sequence}-{context.shot}"
```

### Functor-based format

```json5
"format": "$title({task.name})"
```

### Conditional format

```json5
"format": [
    {
        "expression": "{context.asset_variant}",
        "when": {"key": "context.asset_variant", "exists": true}
    },
    {
        "expression": "$CC({context.asset}, {context.variant})",
        "when": {"keys": ["context.asset", "context.variant"], "exists": true}
    }
]
```

The current loader supports `when` checks based on:

- one `key`,
- multiple `keys`,
- an `exists` boolean.

### Auto-generated formats

If you omit `format`, Silex may synthesize one when either of these is true:

- the segment has a target bound to the raw `value`, or
- the `pattern` is a plain literal without regex metacharacters.

This is useful, but explicit formats are still better for maintainability.

## Templates

Templates are the main way to keep schemas readable.

Example:

```json5
"templates": {
    "version": {
        "pattern": "v([0-9]{3,4})",
        "type": "number",
        "padding": 4,
        "format": "v{value:04d}"
    }
}
```

Then apply the template to a segment:

```json5
"version": {
    "template": "version",
    "endpoint": "task.version"
}
```

Template behavior in the current loader:

- template chains are resolved iteratively,
- local segment values override template values,
- cyclic template references are stopped defensively,
- lifecycle flags like `is_readonly` and `is_deprecated` can come from templates.

Good template candidates:

- versions,
- frame numbers,
- UDIMs,
- asset-variant names,
- common filename structures,
- shared parse and format expression bundles.

## Endpoints

Endpoints are declared on segments, not as a separate top-level block.

Example:

```json5
"frame_file": {
    "pattern": "([a-zA-Z0-9_]+)\\.([0-9]+)\\.([a-zA-Z]+)",
    "endpoint": "shot.frame"
}
```

During schema loading, Silex walks the built segment tree and extracts endpoint names automatically.

Use stable endpoint names that describe semantic targets rather than physical directory names.

Good examples:

- `asset`,
- `asset.scene`,
- `shot.frame`,
- `asset.prism.versioninfo`.

## Switch Branches

Use `switch` when a segment value selects an entirely different child subtree.

Example:

```json5
"tree": {
    "options": ["assets", "shots"],
    "format": "$title({context.tree})",
    "parse": {
        "expressions": ["$lower({value})->tree"],
        "targets": {"context.tree": "tree"}
    },
    "switch": [
        {
            "value": "assets",
            "paths": {
                "asset_dir": {
                    "pattern": "[a-zA-Z0-9]+"
                }
            }
        },
        {
            "value": "shots",
            "paths": {
                "sequence": {
                    "pattern": "seq[0-9]+"
                }
            }
        }
    ]
}
```

Each switch case can also define its own branch-level `endpoint`.

## Partitions

Partitions are an advanced feature for segments that own dynamic child sets.

You can define them as either:

- an object keyed by partition name, or
- an array of partition definitions.

If you use partitions, also review:

- `partition_segmenter`,
- `ordered_partitions`,
- placeholder behavior during formats.

For most filesystem schemas, normal nested `paths` or `switch` branches are simpler and easier to test.

## Configuration

Schema-level `config` is best used for shared variables and case sensitivity.

Example:

```json5
"config": {
    "case_sensitive": false,
    "variables": {
        "lexicon": {
            "classification": {
                "character": ["chr", "character"],
                "prop": ["prp", "prop"]
            }
        }
    }
}
```

The current parser maps these keys into runtime config:

- `variables` -> global variables,
- `functor_variables` -> functor variables,
- `placeholder_variables` -> placeholder variables,
- `case_sensitive` -> regex behavior default.

If you use placeholders extensively, verify their runtime behavior with tests. Placeholder precedence is sensitive to both resolver options and schema definitions.

## Aliases and Components

You do not need to redefine built-in functors for every schema, but authoring schemas often import a shared core file so aliases are available.

Example alias block:

```json5
"aliases": {
    "L": "silex.impl.functors.lexicon.LexiconFunctor",
    "CC": "silex.impl.functors.case_split.SplitCamelCaseFunctor",
    "glob": "silex.impl.functors.glob.GlobFunctor"
}
```

This enables expressions like:

```json5
"$CC({value})->classification:0,asset:0--1"
```

instead of spelling out the full functor path.

## Overrides

Overrides are applied after an inherited schema is built.

Example:

```json5
"overrides": {
    "project": {
        "pattern": "TESTPIPE",
        "is_readonly": true
    }
}
```

Important current behavior:

- override targets are addressed by dot-separated segment path,
- pattern overrides work,
- `is_readonly` and `is_deprecated` overrides are applied,
- a pattern override of `(?!)` effectively disables a branch for formats,
- more advanced override fields exist in the validation schema but should be considered implementation-sensitive unless covered by tests in your project.

If you depend on override behavior beyond pattern and lifecycle flags, add explicit tests before relying on it in production.

## Full Example

This example shows a compact but realistic filesystem schema.

```json5
{
    "uid": "example.assets",
    "import": ["./common", "./silex_core"],
    "pattern": ".*assets.*",
    "root_path": "${SHOW_ROOT}/projects/demo",
    "segmenter": "silex.FilesystemSegmenter",
    "context_filters": [
        {"context.tree": "assets"}
    ],
    "templates": {
        "version_tag": {
            "pattern": "v([0-9]{3})",
            "format": "v{value:03d}",
            "parse": {
                "targets": {
                    "entity.version": {"group": 1, "type": "int"}
                }
            }
        }
    },
    "paths": {
        "assets_root": {
            "pattern": "assets",
            "format": "assets",
            "target": "context.tree",
            "paths": {
                "asset_name": {
                    "pattern": "[a-zA-Z][a-zA-Z0-9_]*",
                    "format": "{context.asset}",
                    "parse": {
                        "expressions": [
                            "$lower({value})->asset"
                        ],
                        "targets": {
                            "context.asset": "asset"
                        }
                    },
                    "endpoint": "asset",
                    "paths": {
                        "fragment": {
                            "pattern": "[a-zA-Z_]+",
                            "target": "entity.fragment",
                            "format": "{entity.fragment}",
                            "paths": {
                                "version": {
                                    "template": "version_tag",
                                    "endpoint": "asset.version",
                                    "paths": {
                                        "scene_file": {
                                            "pattern": "([a-zA-Z0-9_]+)\\.([a-zA-Z0-9]+)",
                                            "format": "{context.asset}_{entity.fragment}.v{entity.version:03d}.{file.ext}",
                                            "parse": {
                                                "targets": {
                                                    "file.name": {"group": 1},
                                                    "file.ext": {"group": 2}
                                                }
                                            },
                                            "endpoint": "asset.scene"
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}
```

This is not intended as a house style. It is intended to show how the pieces fit together in one file.

## Practical Rules

Use these rules to keep schemas maintainable:

1. Keep segment names semantic, not temporary.
2. Prefer templates for repeated regex or version logic.
3. Keep `parse` and `format` symmetrical wherever possible.
4. Use `endpoint` names that reflect what callers want to resolve.
5. Prefer explicit `format` expressions over implicit generation.
6. Keep functor aliases short but unambiguous.
7. Treat `overrides`, partitions, and placeholder-heavy behavior as test-required features.
8. Put shared lexicons and common templates in imported files rather than copying them across schemas.

## Common Failure Modes

If a schema looks valid but does not behave correctly, check these first:

1. `SILEX_SCHEMA_PATH` does not include the directory containing the file.
2. `import` points to a missing schema or uses the wrong relative base.
3. `pattern` is too broad or too narrow, so the schema is never selected.
4. `root_path` resolves differently than expected after environment expansion.
5. `context_filters` do not match the actual dotted keys in your context.
6. `parse.targets` point at the wrong capture groups.
7. `format` expressions reference keys that are not populated at format time.
8. An inherited segment was overridden less deeply than intended.
9. A branch is `is_readonly` or deprecated and you expected it to format.

## Validation and Testing

Recommended validation layers:

1. Validate the file against `schema/silex_schema.json`.
2. Add a minimal parse test for each important endpoint.
3. Add a minimal format test for each important endpoint.
4. Add at least one test around inheritance or overrides if the schema uses them.
5. Add placeholder tests if the schema formats partial or tokenized paths.

The best reference files in this repository are:

- `tests/resources/test_integration.silex` for a compact working example,
- `examples/02_templates_and_shared_variables/` for reusable templates and variables,
- `examples/07_project_filesystem_layout/` for a production-style filesystem tree.

## Related Docs

- `api_guide.md` explains how callers consume schemas through `GenericResolver`.
- `architecture.md` explains how the loader, parser, registry, and resolver interact.
