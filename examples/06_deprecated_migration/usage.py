"""Demonstrate canonical writes and opt-in reads for deprecated paths."""

from silex import GenericResolver, ResolverStatus, SilexParseOptions


def main() -> None:
    """Verify migration behavior stays explicit and testable."""
    canonical_resolver = GenericResolver(schema="example.dir.migration")
    canonical_path = "/show/assets/dragon/work/lookdev"

    # The canonical route should be the only default write target.
    write_result = canonical_resolver.path_from_context(
        {"context": {"asset": "dragon"}, "task": {"name": "lookdev"}},
        endpoint="asset.work",
    )
    assert write_result.status == ResolverStatus.SUCCESS
    assert write_result.resolved_path.replace("\\", "/") == canonical_path

    # Deprecated reads require an explicit opt-in so migration remains intentional.
    deprecated_resolver = GenericResolver(
        schema="example.dir.migration",
        options=SilexParseOptions(include_deprecated=True),
    )
    read_result = deprecated_resolver.context_from_path(
        "/show/assets/dragon/Scenefiles/lookdev",
        endpoint="asset.legacy_scene",
    )
    assert read_result.status == ResolverStatus.SUCCESS
    assert read_result.context["context"]["asset"] == "dragon"
    assert read_result.context["task"]["name"] == "lookdev"


if __name__ == "__main__":
    main()