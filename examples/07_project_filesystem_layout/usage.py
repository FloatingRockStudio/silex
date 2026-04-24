# (C) Copyright 2026 Floating Rock Studio Ltd
# SPDX-License-Identifier: MIT

"""Demonstrate a production-style project tree built from imported templates."""

from silex import GenericResolver, ResolverStatus


def main() -> None:
    """Verify a project/entity/task/version layout resolves in both directions."""
    resolver = GenericResolver(schema="example.dir.project_layout")
    expected_path = "X:/Projects/PROJECTX/03_Production/Assets/TreeA/Lookdev/v0007"

    # Reading the tree shows how the broader layout maps onto structured context.
    read_result = resolver.context_from_path(expected_path, endpoint="project.entity.task.version")
    assert read_result.status == ResolverStatus.SUCCESS
    assert read_result.context["context"]["project"] == "PROJECTX"
    assert read_result.context["context"]["tree"] == "Assets"
    assert read_result.context["context"]["entity"] == "TreeA"
    assert read_result.context["task"]["name"] == "Lookdev"
    assert read_result.context["file"]["version"] == "0007"

    # Writing the same endpoint back protects the example as a living contract.
    write_result = resolver.path_from_context(read_result.context, endpoint="project.entity.task.version")
    assert write_result.status == ResolverStatus.SUCCESS
    assert write_result.resolved_path.replace("\\", "/") == expected_path


if __name__ == "__main__":
    main()