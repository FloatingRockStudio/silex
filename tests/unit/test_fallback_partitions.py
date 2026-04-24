"""
Tests for fallback partition segments with dynamic keys.

These tests focus on:
1. Reading URIs with unknown query parameters using fallback partitions
2. Writing URIs with dynamic context keys using $k and $v placeholders
3. Fallback partition priority (non-fallback matches first)
"""

# Set environment BEFORE any silex imports
import os
from pathlib import Path

test_resources_path = Path(__file__).parent.parent / "resources"
os.environ["SILEX_SCHEMA_PATH"] = str(test_resources_path)

import pytest
from silex import GenericResolver, ResolverStatus, set_verbosity


class TestFallbackPartitions:
    """Test fallback partition processing with dynamic keys."""

    def setup_method(self):
        """Set up for each test."""
        set_verbosity(2)

    def test_fallback_partition_reads_unknown_params(self):
        """Test that fallback partitions catch unknown query parameters."""
        resolver = GenericResolver()

        # URI with unknown query parameters that should match fallback pattern
        uri = "FRA://asset_entity/PROJ/entity/frag/comp?custom=42&unknown=99"
        result = resolver.context_from_path(uri)

        assert result.status == ResolverStatus.SUCCESS
        # Fallback should capture these as dynamic keys in options
        assert result.context["options"]["custom"] == "42"
        assert result.context["options"]["unknown"] == "99"

    def test_fallback_partition_priority(self):
        """Test that non-fallback partitions match before fallback."""
        resolver = GenericResolver()

        # URI with mix of known and unknown parameters
        uri = "FRA://asset_entity/PROJ/entity/frag/comp?v=5&custom=100"
        result = resolver.context_from_path(uri)

        assert result.status == ResolverStatus.SUCCESS
        # Version should match specific version_param pattern (int type)
        assert result.context["options"]["version"] == 5
        assert isinstance(result.context["options"]["version"], int)
        # Custom should match fallback pattern (string type)
        assert result.context["options"]["custom"] == "100"
        assert isinstance(result.context["options"]["custom"], str)

    def test_fallback_partition_with_tags(self):
        """Test that fallback doesn't interfere with array parameters."""
        resolver = GenericResolver()

        # Mix of tag array and fallback parameters
        uri = "FRA://asset_entity/PROJ/entity/frag/comp?tag=approved&tag=final&priority=10"
        result = resolver.context_from_path(uri)

        assert result.status == ResolverStatus.SUCCESS
        # Tags should still be collected as array
        assert result.context["options"]["tags"] == ["approved", "final"]
        # Priority should be captured by fallback
        assert result.context["options"]["priority"] == "10"

    def test_fallback_partition_dynamic_key_from_group(self):
        """Test that dynamic target keys work with group references."""
        resolver = GenericResolver()

        # Multiple custom parameters
        uri = "FRA://asset_entity/PROJ/entity/frag/comp?width=1920&height=1080&depth=32"
        result = resolver.context_from_path(uri)

        assert result.status == ResolverStatus.SUCCESS
        # All custom parameters should be captured
        assert result.context["options"]["width"] == "1920"
        assert result.context["options"]["height"] == "1080"
        assert result.context["options"]["depth"] == "32"

    def test_fallback_partition_write_iteration(self):
        """Test that fallback partitions write using $k/$v iteration."""
        resolver = GenericResolver()

        context = {
            "uri": {"context": "asset_entity"},
            "context": {
                "project": "PROJ",
                "entity": "entity",
                "fragment": "frag",
                "component": "comp",
            },
            "options": {
                "version": 5,
                "custom1": "100",
                "custom2": "200",
                "setting": "high",
            },
        }

        result = resolver.path_from_context(context, endpoint="asset_entity", include_children=True)
        assert result.status == (ResolverStatus.SUCCESS | ResolverStatus.AMBIGUOUS)

        path = result.resolved_path
        # Known partition should be written
        assert "v=5" in path
        # Custom partitions should be written via fallback iteration
        assert "custom1=100" in path
        assert "custom2=200" in path
        assert "setting=high" in path

    def test_fallback_partition_empty_options(self):
        """Test that missing options don't cause fallback to fail."""
        resolver = GenericResolver()

        # Context without custom options
        context = {
            "uri": {"context": "asset_entity"},
            "context": {
                "project": "PROJ",
                "entity": "entity",
                "fragment": "frag",
                "component": "comp",
            },
        }

        result = resolver.path_from_context(context, endpoint="asset_entity", include_children=True)
        assert result.status == ResolverStatus.SUCCESS
        # Should succeed even without options (fallback produces nothing)

    def test_fallback_partition_mixed_read_write(self):
        """Test round-trip with fallback partitions."""
        resolver = GenericResolver()

        # Start with a URI containing custom parameters (all numeric to match fallback pattern)
        original_uri = "FRA://asset_entity/TEST/myasset/geo/low?v=3&lod=5&quality=100"

        # Read it
        read_result = resolver.context_from_path(original_uri)
        assert read_result.status == ResolverStatus.SUCCESS

        # Verify all parameters were captured
        assert read_result.context["options"]["version"] == 3
        assert read_result.context["options"]["lod"] == "5"
        assert read_result.context["options"]["quality"] == "100"

        # Write it back
        write_result = resolver.path_from_context(read_result.context, endpoint="asset_entity", include_children=True)
        assert write_result.status == (ResolverStatus.SUCCESS | ResolverStatus.AMBIGUOUS)

        # Path should contain all parameters
        path = write_result.resolved_path
        assert "v=3" in path
        assert "lod=5" in path
        assert "quality=100" in path

    def test_fallback_partition_only_numeric_values(self):
        """Test that fallback pattern only matches numeric values per schema."""
        resolver = GenericResolver()

        # According to schema: "([_a-zA-Z0-9]+)=([0-9]+)" - value must be numeric
        uri = "FRA://asset_entity/PROJ/entity/frag/comp?numeric=123&nonnum=abc"
        result = resolver.context_from_path(uri)

        assert result.status == ResolverStatus.SUCCESS
        # Numeric value should match fallback
        assert result.context["options"]["numeric"] == "123"
        # Non-numeric value should NOT match fallback pattern (no match)
        assert "nonnum" not in result.context.get("options", {})

    def test_fallback_write_excludes_known_options(self):
        """Test that known options are written by their specific partitions, not fallback."""
        resolver = GenericResolver()

        context = {
            "uri": {"context": "asset_entity"},
            "context": {
                "project": "PROJ",
                "entity": "test",
                "fragment": "tex",
                "component": "diff",
            },
            "options": {
                "version": 10,  # Should use version_param partition
                "tags": ["work"],  # Should use tag_param partition
                "custom": "50",  # Should use fallback
            },
        }

        result = resolver.path_from_context(context, endpoint="asset_entity", include_children=True)
        assert result.status == (ResolverStatus.SUCCESS | ResolverStatus.AMBIGUOUS)

        path = result.resolved_path
        # Version should be written by version_param
        assert "v=10" in path
        # Tag should be written by tag_param
        assert "tag=work" in path
        # Custom should be written by fallback
        assert "custom=50" in path

    def test_multiple_fallback_params_order(self):
        """Test that multiple fallback parameters are written in consistent order."""
        resolver = GenericResolver()

        context = {
            "uri": {"context": "asset_entity"},
            "context": {
                "project": "PROJ",
                "entity": "test",
                "fragment": "tex",
                "component": "diff",
            },
            "options": {
                "alpha": "1",
                "beta": "2",
                "gamma": "3",
            },
        }

        result = resolver.path_from_context(context, endpoint="asset_entity", include_children=True)
        assert result.status == (ResolverStatus.SUCCESS | ResolverStatus.AMBIGUOUS)

        path = result.resolved_path
        # All custom parameters should be present
        assert "alpha=1" in path
        assert "beta=2" in path
        assert "gamma=3" in path

    def test_fallback_with_all_partition_types(self):
        """Test fallback alongside all other partition types."""
        resolver = GenericResolver()

        uri = "FRA://asset_entity/PROJ/ent/frag/comp?v=5&tag=a&tag=b&custom1=99&custom2=88"
        result = resolver.context_from_path(uri)

        assert result.status == ResolverStatus.SUCCESS
        # Specific version partition
        assert result.context["options"]["version"] == 5
        # Array tag partition
        assert result.context["options"]["tags"] == ["a", "b"]
        # Fallback partitions
        assert result.context["options"]["custom1"] == "99"
        assert result.context["options"]["custom2"] == "88"

    def test_no_fallback_for_component_endpoint(self):
        """Test that asset_component endpoint does NOT have fallback partition."""
        resolver = GenericResolver()

        uri = "FRA://asset_component/PROJ/ent/frag/comp?special=777"
        result = resolver.context_from_path(uri, endpoint="asset_component")

        assert result.status == ResolverStatus.SUCCESS
        # asset_component endpoint doesn't have fallback, so special param is NOT captured
        assert "special" not in result.context.get("options", {})


@pytest.mark.parametrize(
    "custom_params,expected_options",
    [
        ("x=1&y=2", {"x": "1", "y": "2"}),
        ("setting1=100&setting2=200&setting3=300", {"setting1": "100", "setting2": "200", "setting3": "300"}),
        ("single=999", {"single": "999"}),
    ],
)
def test_parametrized_fallback_options(custom_params, expected_options):
    """Test various custom parameter combinations via fallback."""
    resolver = GenericResolver()

    uri = f"FRA://asset_entity/PROJ/test/tex/diff?{custom_params}"
    result = resolver.context_from_path(uri)

    assert result.status == ResolverStatus.SUCCESS
    for key, expected_value in expected_options.items():
        assert result.context["options"][key] == expected_value
