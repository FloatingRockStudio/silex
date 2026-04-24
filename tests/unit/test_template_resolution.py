# (C) Copyright 2026 Floating Rock Studio Ltd
# SPDX-License-Identifier: MIT

"""
Unit tests for template resolution methods in GenericResolver.
"""

import pytest
from silex import GenericResolver


class TestTemplateResolution:
    """Test parse_template_value and format_template_value methods."""

    @pytest.fixture
    def resolver(self):
        """Create a resolver instance."""
        return GenericResolver()

    def test_parse_template_value_asset_variant(self, resolver):
        """Test extracting context from an asset variant string."""
        # Test character asset
        result = resolver.parse_template_value("chrAlexBase", "asset_variant")

        assert result is not None
        assert "classification" in result
        assert "asset" in result
        assert "variant" in result
        assert result["classification"] == "character"
        assert result["asset"] == "alex"
        assert result["variant"] == "base"

    def test_parse_template_value_prop_asset(self, resolver):
        """Test extracting context from a prop asset variant."""
        result = resolver.parse_template_value("prpRockA", "asset_variant")

        assert result is not None
        assert result["classification"] == "prop"
        assert result["asset"] == "rock"
        assert result["variant"] == "a"

    def test_parse_template_value_env_asset(self, resolver):
        """Test extracting context from an environment asset variant."""
        result = resolver.parse_template_value("envForestClearing", "asset_variant")

        assert result is not None
        assert result["classification"] == "environment"
        assert result["asset"] == "forest"
        assert result["variant"] == "clearing"
        assert "variant" in result

    def test_format_template_value_asset_variant(self, resolver):
        """Test generating an asset variant from context."""
        context = {
            "context": {
                "classification": "character",
                "asset": "alex",
                "variant": "base"
            }
        }

        result = resolver.format_template_value(context, "asset_variant")

        assert result == "chrAlexBase"

    def test_format_template_value_prop(self, resolver):
        """Test generating a prop asset variant from context."""
        context = {
            "context": {
                "classification": "prop",
                "asset": "rock",
                "variant": "a"
            }
        }

        result = resolver.format_template_value(context, "asset_variant")

        assert result == "prpRockA"

    def test_roundtrip_asset_variant(self, resolver):
        """Test that value -> context -> value roundtrips correctly."""
        original_value = "chrAlexBase"

        # Parse to context
        context = resolver.parse_template_value(original_value, "asset_variant")
        assert context is not None

        # Generate back to value (wrap context for format_template_value)
        regenerated_value = resolver.format_template_value({"context": context}, "asset_variant")

        assert regenerated_value == original_value

    def test_parse_template_value_nonexistent_template(self, resolver):
        """Test that nonexistent template returns empty dict."""
        result = resolver.parse_template_value("somevalue", "nonexistent_template")

        assert result == {}

    def test_format_template_value_nonexistent_template(self, resolver):
        """Test that nonexistent template returns empty string."""
        context = {"somekey": "somevalue"}

        result = resolver.format_template_value(context, "nonexistent_template")

        assert result == ""

    def test_parse_template_value_with_schema_filter(self, resolver):
        """Test using schema filter to find template."""
        # Should find template in common or files.projects schemas
        result = resolver.parse_template_value("chrAlexBase", "asset_variant", schema="*")

        assert result is not None
        assert "classification" in result

    def test_format_template_value_with_schema_filter(self, resolver):
        """Test using schema filter for value generation."""
        context = {
            "context": {
                "classification": "character",
                "asset": "alex",
                "variant": "base"
            }
        }

        result = resolver.format_template_value(context, "asset_variant", schema="*")

        assert result == "chrAlexBase"

    def test_parse_template_value_sequence_shot(self, resolver):
        """Test extracting context from sequence-shot pattern."""
        result = resolver.parse_template_value("010-020", "sequence_shot")

        assert result is not None
        assert "sequence" in result
        assert "shot" in result
        assert result["sequence"] == "010"
        assert result["shot"] == "020"

    def test_format_template_value_sequence_shot(self, resolver):
        """Test generating sequence-shot pattern from context."""
        context = {
            "context": {
                "sequence": "010",
                "shot": "020"
            }
        }

        result = resolver.format_template_value(context, "sequence_shot")

        assert result == "010-020"
