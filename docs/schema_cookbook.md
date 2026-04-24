# Schema Cookbook

This guide provides complete `.silex` examples you can adapt directly. It complements `schema_authoring.md`, which remains the authoritative language guide for the full schema surface.

## Example 1: Minimal Filesystem Schema

Use this as the baseline for a conventional disk-backed resolver.

```json5
{
	"uid": "example.filesystem",
	"pattern": ".*projects.*",
	"root_path": "/studio/projects",
	"segmenter": "silex.FilesystemSegmenter",
	"context_filters": [
		{"context.project": ".*"}
	],
	"paths": {
		"project": {
			"pattern": "[A-Z0-9_]+",
			"endpoint": "context.project",
			"target": "context.project",
			"format": "{context.project}",
			"paths": {
				"assets_root": {
					"pattern": "assets",
					"format": "assets",
					"paths": {
						"asset": {
							"pattern": "[A-Za-z0-9_]+",
							"target": "context.asset",
							"format": "{context.asset}",
							"endpoint": "context.asset",
							"paths": {
								"task": {
									"pattern": "[a-z]+",
									"target": "task.name",
									"format": "{task.name}",
									"paths": {
										"version": {
											"pattern": "v([0-9]{3})",
											"format": "v{task.version:03d}",
											"parse": {
												"targets": {
													"task.version": {"group": 1, "type": "int"}
												}
											},
											"endpoint": "asset.task.version"
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

Why this works:

- `root_path` anchors traversal under one filesystem root.
- `context_filters` let the resolver discard the schema quickly when no `context.project` is present.
- endpoints are declared where callers actually want to target traversal.

## Example 2: Templates And Shared Variables

Use templates when version, frame, or naming logic repeats across multiple branches.

```json5
{
	"uid": "example.templates",
	"pattern": ".*render.*",
	"root_path": "/show/render",
	"segmenter": "silex.FilesystemSegmenter",
	"templates": {
		"version_segment": {
			"pattern": "v([0-9]{4})",
			"format": "v{value:04d}",
			"parse": {
				"targets": {
					"value": {"group": 1, "type": "int"}
				}
			}
		},
		"asset_variant": {
			"pattern": "[A-Za-z0-9]+",
			"write_update_keys": [
				"context.asset_variant",
				"context.asset",
				"context.variant",
				"context.classification"
			],
			"format": [
				{
					"expression": "{context.asset_variant}",
					"when": {"key": "context.asset_variant", "exists": true}
				},
				{
					"expression": "$CC({context.asset}, {context.variant})",
					"when": {"keys": ["context.asset", "context.variant"], "exists": true}
				}
			],
			"parse": {
				"expressions": [
					"$CC({value})->classification_token:0,asset:0--2,variant:-1:?",
					"$L({classification_token})->classification",
					"$lower({variant})->variant"
				],
				"targets": {
					"context.asset_variant": "value",
					"context.asset": "asset",
					"context.variant": "variant",
					"context.classification": "classification"
				}
			}
		}
	},
	"config": {
		"variables": {
			"lexicon": {
				"classification": {
					"character": ["chr", "character"],
					"prop": ["prp", "prop"],
					"environment": ["env", "environment"]
				}
			}
		}
	},
	"paths": {
		"asset_variant": {
			"template": "asset_variant",
			"endpoint": "asset.variant",
			"paths": {
				"version": {
					"template": "version_segment",
					"target": "task.version",
					"endpoint": "asset.variant.version"
				}
			}
		}
	}
}
```

Why this works:

- shared naming logic lives in a reusable template instead of being copied into each branch
- lexicon configuration keeps case-splitting and normalization rules in one place
- `write_update_keys` makes format traversal practical when the canonical value can be assembled from multiple context keys

## Example 3: URI Schema With Query Parameters

Use a URI schema when the canonical identifier is not a filesystem path.

```json5
{
	"uid": "example.uri",
	"root_path": "FRA://",
	"pattern": "(?i)FRA://.*",
	"segmenters": [
		"silex.URISegmenter",
		"silex.QueryParamsSegmenter"
	],
	"segmenter": "silex.URISegmenter",
	"paths": {
		"resource_type": {
			"pattern": "(asset|shot)",
			"format": "{uri.resource_type}",
			"parse": {
				"targets": {
					"uri.resource_type": {"group": 1}
				}
			},
			"paths": {
				"project": {
					"pattern": "[A-Z0-9_]+",
					"target": "context.project",
					"format": "{context.project}",
					"paths": {
						"entity": {
							"pattern": "[A-Za-z0-9_]+",
							"target": "context.entity",
							"format": "{context.entity}",
							"endpoint": "uri.entity",
							"paths": {
								"query": {
									"pattern": "\\?(.+)",
									"partition_segmenter": "silex.QueryParamsSegmenter",
									"partitions": [
										{
											"name": "version_param",
											"pattern": "v=([0-9]+)",
											"format": "v={options.version}",
											"parse": {
												"targets": {
													"options.version": {"group": 1, "type": "int"}
												}
											}
										},
										{
											"name": "tag_param",
											"pattern": "tag=([^&]+)",
											"format": "tag={options.tags[*]}",
											"parse": {
												"targets": {
													"options.tags": {"group": 1, "is_array": true}
												}
											}
										},
										{
											"name": "any_param",
											"pattern": "([_a-zA-Z0-9]+)=([^&]+)",
											"format": "{options.$k}={options.$v}",
											"parse": {
												"targets": {
													"options.{group[1]}": {"group": 2}
												}
											},
											"is_fallback": true
										}
									]
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

Why this works:

- the URI segmenter handles the path-style portion of the URI
- query parameters are modeled as partitions, not as fake path segments
- the fallback partition allows controlled extensibility for unknown options

## Example 4: Inheritance And Overrides

Use inheritance when you want a consumer-specific schema to start from a stable base and change only a narrow subtree.

Base schema:

```json5
{
	"uid": "example.base_publish",
	"pattern": ".*publish.*",
	"root_path": "/show/publish",
	"segmenter": "silex.FilesystemSegmenter",
	"paths": {
		"project": {
			"pattern": "[A-Z0-9_]+",
			"target": "context.project",
			"format": "{context.project}",
			"paths": {
				"asset": {
					"pattern": "[A-Za-z0-9_]+",
					"target": "context.asset",
					"format": "{context.asset}",
					"paths": {
						"publish_root": {
							"pattern": "publish",
							"format": "publish",
							"paths": {
								"version": {
									"pattern": "v([0-9]{3})",
									"format": "v{file.version:03d}",
									"parse": {
										"targets": {
											"file.version": {"group": 1, "type": "int"}
										}
									},
									"endpoint": "asset.publish.version"
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

Derived schema:

```json5
{
	"uid": "example.client_publish",
	"inherits": "example.base_publish",
	"overrides": {
		"project.asset.publish_root.version": {
			"format": "ver_{file.version:03d}",
			"pattern": "ver_([0-9]{3})"
		}
	}
}
```

Why this works:

- the base schema stays simple and reusable
- the override is narrowly targeted to one segment path
- the derived schema changes the naming convention without duplicating the whole tree

## Example 5: Deprecation And Read-Only Migration

Use lifecycle flags when you need to keep legacy parses alive while moving formats to a new canonical branch.

```json5
{
	"uid": "example.migration",
	"pattern": ".*assets.*",
	"root_path": "/show/assets",
	"segmenter": "silex.FilesystemSegmenter",
	"paths": {
		"asset": {
			"pattern": "[A-Za-z0-9_]+",
			"target": "context.asset",
			"format": "{context.asset}",
			"paths": {
				"canonical_work": {
					"pattern": "work",
					"format": "work",
					"endpoint": "asset.work",
					"paths": {
						"task": {
							"pattern": "[a-z]+",
							"target": "task.name",
							"format": "{task.name}"
						}
					}
				},
				"legacy_scenefiles": {
					"pattern": "Scenefiles",
					"format": "Scenefiles",
					"endpoint": "asset.legacy_scene",
					"is_deprecated": true,
					"is_readonly": true,
					"paths": {
						"task": {
							"pattern": "[a-z]+",
							"target": "task.name",
							"format": "{task.name}"
						}
					}
				}
			}
		}
	}
}
```

Why this works:

- canonical formats go to `asset.work`
- old paths can still resolve through `asset.legacy_scene`
- callers can inspect `used_deprecated_traversal` and migrate away from legacy routes intentionally

## Recommended Authoring Pattern

For new schemas:

1. Start with one stable parse/format endpoint and test it first.
2. Introduce templates only after the direct segment behavior is correct.
3. Use inheritance for structural reuse and overrides for narrow specializations.
4. Add deprecation and fallback behavior only when there is a concrete migration or extensibility need.
5. Keep examples and tests self-contained so the schema stays understandable after surrounding files change.