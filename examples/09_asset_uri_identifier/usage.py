"""Demonstrate an asset-focused URI identifier with structured options."""

from silex import GenericResolver, ResolverStatus


def main() -> None:
    """Verify the asset URI example resolves task and option query params."""
    resolver = GenericResolver(schema="example.dir.asset_uri")
    expected_uri = "FRA://asset/PROJ1/chrHeroMain?version=7&task=lookdev&branch=final"

    # Read the URI to show how task and fallback query params populate context.
    read_result = resolver.context_from_path(expected_uri, endpoint="uri.asset")
    assert read_result.status == ResolverStatus.SUCCESS
    assert read_result.context["context"]["project"] == "PROJ1"
    assert read_result.context["context"]["asset_variant"] == "chrHeroMain"
    assert read_result.context["task"]["name"] == "lookdev"
    assert read_result.context["options"]["version"] == 7
    assert read_result.context["options"]["branch"] == "final"

    # Write the same structured context back into a canonical asset URI.
    write_result = resolver.path_from_context(read_result.context, endpoint="uri.asset", include_children=True)
    assert write_result.status == ResolverStatus.SUCCESS
    assert write_result.resolved_path == expected_uri


if __name__ == "__main__":
    main()