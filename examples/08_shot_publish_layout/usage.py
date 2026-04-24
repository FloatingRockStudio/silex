"""Demonstrate a shot-oriented publish layout with a shared shot template."""

from silex import GenericResolver, ResolverStatus


def main() -> None:
    """Verify a sequence/shot publish path remains round-trip safe."""
    resolver = GenericResolver(schema="example.dir.shot_publish")
    expected_path = "X:/Projects/PROJ1/03_Production/Shots/010/010-020/publish/compMain/v003"

    # Read first so the example explains how the shot template populates context keys.
    read_result = resolver.context_from_path(expected_path, endpoint="shot.publish.version")
    assert read_result.status == ResolverStatus.SUCCESS
    assert read_result.context["context"]["project"] == "PROJ1"
    assert read_result.context["context"]["sequence"] == "010"
    assert read_result.context["context"]["shot"] == "020"
    assert read_result.context["entity"]["name"] == "compMain"
    assert read_result.context["entity"]["version"] == "003"

    # Write the same endpoint back to keep the publish layout example executable.
    write_result = resolver.path_from_context(read_result.context, endpoint="shot.publish.version")
    assert write_result.status == ResolverStatus.SUCCESS
    assert write_result.resolved_path.replace("\\", "/") == expected_path


if __name__ == "__main__":
    main()