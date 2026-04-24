"""Demonstrate imported templates, variables, and core functor aliases."""

from silex import GenericResolver, ResolverStatus


def main() -> None:
    """Show that imported config and templates can still round-trip cleanly."""
    resolver = GenericResolver(schema="example.dir.templates")
    expected_path = "/show/render/chrHeroMain/v0012"

    # Writing from context exercises the imported template and lexicon setup.
    write_result = resolver.path_from_context(
        {"context": {"asset_variant": "chrHeroMain"}, "task": {"version": 12}},
        endpoint="asset.variant.version",
    )
    assert write_result.status == ResolverStatus.SUCCESS
    assert write_result.resolved_path.replace("\\", "/") == expected_path

    # Reading the path back proves the example remains a valid executable reference.
    read_result = resolver.context_from_path(expected_path, endpoint="asset.variant.version")
    assert read_result.status == ResolverStatus.SUCCESS
    assert read_result.context["context"]["asset_variant"] == "chrHeroMain"
    assert read_result.context["task"]["version"] in (12, "0012")


if __name__ == "__main__":
    main()