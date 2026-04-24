# Silex Examples

This directory contains executable reference examples with increasing complexity.

Each numbered directory contains:

- `schema.silex`: the primary schema under discussion
- `usage.py`: an executable assertion script that demonstrates context-to-path and path-to-context usage
- optional support files such as `common.silex` or `silex_core.silex` when the example needs imported templates, config, or functor aliases

Available examples:

- `01_minimal_filesystem`: smallest practical filesystem schema
- `02_templates_and_shared_variables`: imported templates, lexicon variables, and core aliases
- `03_uri_query_params`: URI segmenting with query param partitions
- `04_base_publish`: reusable publish tree for inheritance
- `05_client_publish_override`: client-specific publish variant with a customized version token
- `06_deprecated_migration`: canonical vs legacy read-only migration
- `07_project_filesystem_layout`: project, asset, task, and version layout
- `08_shot_publish_layout`: sequence and shot oriented publish layout
- `09_asset_uri_identifier`: asset-centric URI identifiers with options
- `10_sceptre_context_path`: short context paths derived from project trees
- `11_custom_functor`: schema-declared custom Python functor with an executable registration example
- `12_shot_publish_override`: inherited shot publish layout that customizes only the publish folder token

Pytest executes every `usage.py` file through `tests/integration/test_examples.py`, so these examples are both instructional and regression-tested.
