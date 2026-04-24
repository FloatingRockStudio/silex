"""Demonstrate the smallest practical filesystem round-trip."""

from silex import GenericResolver, ResolverStatus


def main() -> None:
    """Resolve a filesystem path into context and build it back again."""
    resolver = GenericResolver(schema="example.dir.filesystem")
    expected_path = "/studio/projects/SHOW01/assets/heroAsset/model/v003"

    # Read the path first so the example documents which keys are populated.
    read_result = resolver.context_from_path(expected_path, endpoint="asset.task.version")
    assert read_result.status == ResolverStatus.SUCCESS
    assert read_result.context["context"]["project"] == "SHOW01"
    assert read_result.context["context"]["asset"] == "heroAsset"
    assert read_result.context["task"]["name"] == "model"
    assert read_result.context["task"]["version"] == "003"

    # Write the same endpoint back to prove the schema is bidirectional.
    write_result = resolver.path_from_context(read_result.context, endpoint="asset.task.version")
    assert write_result.status == ResolverStatus.SUCCESS
    assert write_result.resolved_path.replace("\\", "/") == expected_path


if __name__ == "__main__":
    main()