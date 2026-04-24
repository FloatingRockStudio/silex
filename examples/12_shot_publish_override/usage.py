"""Demonstrate inheritance by customizing one nested segment in a shot layout."""

from silex import GenericResolver, ResolverStatus


def main() -> None:
    """Verify an inherited shot schema can override only the publish folder token."""
    resolver = GenericResolver(schema="example.dir.shot_publish_override")
    expected_path = "X:/Projects/PROJ1/03_Production/Shots/010/010-020/delivery/compMain/v003"

    # Write the inherited endpoint so the single customized segment is easy to see.
    write_result = resolver.path_from_context(
        {
            "context": {"project": "PROJ1", "sequence": "010", "shot": "020"},
            "entity": {"name": "compMain", "version": 3},
        },
        endpoint="shot.publish.version",
    )
    assert write_result.status == ResolverStatus.SUCCESS
    assert write_result.resolved_path.replace("\\", "/") == expected_path

    # Read it back to prove the inherited tree still resolves the same endpoint correctly.
    read_result = resolver.context_from_path(expected_path, endpoint="shot.publish.version")
    assert read_result.status == ResolverStatus.SUCCESS
    assert read_result.context["context"]["sequence"] == "010"
    assert read_result.context["context"]["shot"] == "020"
    assert read_result.context["entity"]["name"] == "compMain"
    assert read_result.context["entity"]["version"] == "003"


if __name__ == "__main__":
    main()