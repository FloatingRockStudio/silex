# (C) Copyright 2026 Floating Rock Studio Ltd
# SPDX-License-Identifier: MIT

"""
Tests for partition processing within URI segments.

These tests specifically focus on the partition processing logic - how query parameters
are split by & and processed as partitions within a segment.
"""

# Set environment BEFORE any silex imports
import os
from pathlib import Path

test_resources_path = Path(__file__).parent.parent / "resources"
os.environ["SILEX_SCHEMA_PATH"] = str(test_resources_path)

import pytest
from silex import GenericResolver, ResolverStatus, set_verbosity


class TestPartitionProcessing:
    """Test partition processing within URI segments."""

    def setup_method(self):
        """Set up for each test."""
        set_verbosity(2)

    def test_single_partition_in_segment(self):
        """Test processing a single partition within a segment."""
        resolver = GenericResolver()

        # Single partition: only version parameter
        uri = "FRA://asset_entity/PROJ/entity/frag/comp?v=5"
        result = resolver.context_from_path(uri)

        assert result.status == ResolverStatus.SUCCESS
        assert result.context["options"]["version"] == 5

    def test_multiple_partitions_in_segment(self):
        """Test processing multiple partitions within a single segment."""
        resolver = GenericResolver()

        # Multiple partitions separated by &
        uri = "FRA://asset_entity/PROJ/entity/frag/comp?v=5&udim=1001&ext=exr"
        result = resolver.context_from_path(uri)

        assert result.status == ResolverStatus.SUCCESS
        assert result.context["options"]["version"] == 5
        assert result.context["options"]["udim"] == "1001"
        assert result.context["options"]["ext"] == "exr"

    def test_partition_array_handling(self):
        """Test how partitions handle array values (multiple same parameters)."""
        resolver = GenericResolver()

        # Multiple tag partitions should be collected into array
        uri = "FRA://asset_component/TEST/asset/geo/low?tag=approved&tag=final&tag=published"
        result = resolver.context_from_path(uri)

        assert result.status == ResolverStatus.SUCCESS
        assert result.context["options"]["tags"] == ["approved", "final", "published"]

    def test_partition_type_conversion(self):
        """Test type conversion in partition processing."""
        resolver = GenericResolver()

        uri = "FRA://sequence/PROJ/seq001?v=42&status=active"
        result = resolver.context_from_path(uri)

        assert result.status == ResolverStatus.SUCCESS
        # Version should be converted to int
        assert isinstance(result.context["options"]["version"], int)
        assert result.context["options"]["version"] == 42
        # Status should remain string
        assert isinstance(result.context["options"]["status"], str)

    def test_partition_pattern_matching(self):
        """Test how partitions match against defined patterns."""
        resolver = GenericResolver()

        # This should match version_param pattern but not others
        uri = "FRA://asset_entity/PROJ/test/tex/diff?v=99&unknown=ignored"
        result = resolver.context_from_path(uri)

        assert result.status == ResolverStatus.SUCCESS
        assert result.context["options"]["version"] == 99
        # Unknown parameter should be ignored (no pattern match)
        assert "unknown" not in result.context.get("options", {})

    def test_partition_write_reconstruction(self):
        """Test writing partitions back to segment."""
        resolver = GenericResolver()

        context = {
            "uri": {"context": "asset_entity"},
            "context": {
                "project": "WRITE",
                "entity": "testEntity",
                "fragment": "texture",
                "component": "diffuse",
            },
            "options": {
                "version": 10,
                "udim": "1002",
                "ext": "tiff",
                "tags": ["work", "latest"],
            },
        }

        result = resolver.path_from_context(context, endpoint="asset_entity", include_children=True)
        assert result.status == (ResolverStatus.SUCCESS | ResolverStatus.AMBIGUOUS)

        # Check that all partitions are reconstructed
        path = result.resolved_path
        assert "v=10" in path
        assert "udim=1002" in path
        assert "ext=tiff" in path
        assert "tag=work" in path
        assert "tag=latest" in path

    def test_empty_partition_segment(self):
        """Test handling segment without partitions."""
        resolver = GenericResolver()

        # No query parameters
        uri = "FRA://asset_entity/NOPARAM/entity/frag/comp"
        result = resolver.context_from_path(uri)

        assert result.status == ResolverStatus.SUCCESS
        assert result.context["context"]["project"] == "NOPARAM"
        # Should have no options when no partitions
        assert "options" not in result.context or not result.context.get("options")

    def test_partition_order_independence(self):
        """Test that partition order doesn't matter."""
        resolver = GenericResolver()

        # Different parameter orders should produce same result
        uri1 = "FRA://asset_component/ORD/test/tex/diff?v=5&ext=jpg&udim=1001"
        uri2 = "FRA://asset_component/ORD/test/tex/diff?udim=1001&v=5&ext=jpg"

        result1 = resolver.context_from_path(uri1)
        result2 = resolver.context_from_path(uri2)

        assert result1.status == ResolverStatus.SUCCESS
        assert result2.status == ResolverStatus.SUCCESS
        assert result1.context["options"] == result2.context["options"]

    def test_different_partition_types_per_context(self):
        """Test that different URI contexts have different partition types."""
        resolver = GenericResolver()

        # Asset URIs have version, udim, ext, tag partitions
        asset_uri = "FRA://asset_entity/PART/asset/tex/diff?v=1&udim=1001"
        asset_result = resolver.context_from_path(asset_uri)

        # Shot URIs have frame, task partitions
        shot_uri = "FRA://shot/PART/seq001/shot001?frame=100&task=anim"
        shot_result = resolver.context_from_path(shot_uri)

        assert asset_result.status == ResolverStatus.SUCCESS
        assert shot_result.status == ResolverStatus.SUCCESS

        # Asset should have version and udim
        assert "version" in asset_result.context["options"]
        assert "udim" in asset_result.context["options"]

        # Shot should have frame and task
        assert "frame" in shot_result.context["options"]
        assert "task" in shot_result.context["options"]

    def test_malformed_partition_handling(self):
        """Test handling of malformed partitions."""
        resolver = GenericResolver()

        # Valid segments with some malformed partitions
        uri = "FRA://asset_entity/MAL/test/tex/diff?v=5&=invalid&valid=ok&ext=jpg"
        result = resolver.context_from_path(uri)

        assert result.status == ResolverStatus.SUCCESS
        # Valid partitions should still work
        assert result.context["options"]["version"] == 5
        assert result.context["options"]["ext"] == "jpg"

    def test_partition_segment_vs_regular_segment(self):
        """Test the difference between partition segments and regular segments."""
        resolver = GenericResolver()

        uri = "FRA://asset_entity/SEG/entity/fragment/component?v=1&ext=png"
        result = resolver.context_from_path(uri)

        assert result.status == ResolverStatus.SUCCESS

        # Regular segments should be in context hierarchy
        assert result.context["context"]["project"] == "SEG"  # Regular segment
        assert result.context["context"]["entity"] == "entity"  # Regular segment
        assert result.context["context"]["fragment"] == "fragment"  # Regular segment
        assert result.context["context"]["component"] == "component"  # Regular segment

        # Partitions should be in options
        assert result.context["options"]["version"] == 1  # Partition
        assert result.context["options"]["ext"] == "png"  # Partition

    def test_nested_partition_processing(self):
        """Test that partition processing works consistently across different contexts."""
        resolver = GenericResolver()

        # Test the same partition logic works in different URI contexts
        contexts_and_uris = [
            ("asset_entity", "FRA://asset_entity/NEST/ent/frag/comp?v=1&ext=abc"),
            ("asset_component", "FRA://asset_component/NEST/ent/frag/comp?v=1&ext=abc"),
            ("sequence", "FRA://sequence/NEST/seq001?v=1"),
            ("shot", "FRA://shot/NEST/seq001/shot001?frame=1"),
        ]

        for context_type, uri in contexts_and_uris:
            result = resolver.context_from_path(uri)
            assert result.status == ResolverStatus.SUCCESS
            assert result.context["uri"]["context"] == context_type

            # All should have some form of versioning or numbering
            options = result.context.get("options", {})
            assert len(options) > 0  # Should have extracted some partitions


@pytest.mark.parametrize(
    "partition_string,expected_count",
    [
        ("v=1", 1),  # Single partition
        ("v=1&ext=jpg", 2),  # Two partitions
        ("v=1&ext=jpg&udim=1001", 3),  # Three partitions
        ("tag=a&tag=b&tag=c", 1),  # Multiple same partition = 1 array option
        ("v=1&ext=jpg&tag=a&tag=b", 3),  # Mixed: version, ext, tags array
    ],
)
def test_parametrized_partition_counts(partition_string, expected_count):
    """Test that correct number of options are extracted from partitions."""
    resolver = GenericResolver()

    uri = f"FRA://asset_entity/COUNT/test/tex/diff?{partition_string}"
    result = resolver.context_from_path(uri)

    assert result.status == ResolverStatus.SUCCESS
    options = result.context.get("options", {})
    assert len(options) == expected_count


@pytest.mark.parametrize(
    "uri_with_partitions,expected_partition_data",
    [
        ("FRA://asset_entity/PAR/test/tex/diff?v=5&udim=1001", {"version": 5, "udim": "1001"}),
        ("FRA://shot/PAR/seq001/shot001?frame=123&task=lighting", {"frame": 123, "task": "lighting"}),
        ("FRA://sequence/PAR/seq042?v=7&status=review", {"version": 7, "status": "review"}),
    ],
)
def test_parametrized_partition_extraction(uri_with_partitions, expected_partition_data):
    """Test extraction of specific partition data."""
    resolver = GenericResolver()

    result = resolver.context_from_path(uri_with_partitions)

    assert result.status == ResolverStatus.SUCCESS
    options = result.context["options"]

    for key, expected_value in expected_partition_data.items():
        assert options[key] == expected_value
