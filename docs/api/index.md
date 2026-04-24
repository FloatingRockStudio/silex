# API Reference

The packaged build generates a Doxygen HTML site under `docs/html/` during the standard build and release flow.

Use `../html/index.html` as the entry point to the rendered API and documentation site.

The Doxygen build consumes:

- the public headers under `source/silex_core/include/silex`
- the binding sources under `source/silex_core/bindings`
- the release-facing Markdown documentation under `docs/`