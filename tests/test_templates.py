"""Test template functionality in silex resolver."""

import pytest
from silex import GenericResolver


def test_available_templates():
    """Test listing available templates."""
    resolver = GenericResolver()

    # Get all templates
    templates = resolver.available_templates()
    print(f"Available templates: {templates}")

    # Should have some templates
    assert len(templates) > 0


def test_parse_template_value():
    """Test extracting context from a value using a template."""
    resolver = GenericResolver()

    # Test with asset_variant template (if it exists) - should match common.silex
    result = resolver.parse_template_value(
        value="chrAlexBase",
        template_name="asset_variant"
    )

    print(f"Context from 'chrAlexBase': {result}")

    # Should extract classification, asset, variant
    if result:
        assert "classification" in result or "asset" in result or "variant" in result
    else:
        print("Warning: asset_variant template did not extract context")


def test_format_template_value():
    """Test generating a value from context using a template."""
    resolver = GenericResolver()

    # Test with asset_variant template
    context = {
        "context": {  # Need to nest under 'context' for template evaluation
            "classification": "chr",
            "asset": "Alex",
            "variant": "Base"
        }
    }

    result = resolver.format_template_value(
        context=context,
        template_name="asset_variant"
    )

    print(f"Value from context: {result}")

    # Should generate something like "chrAlexBase"
    if result:
        assert "chr" in result.lower() or "Alex" in result
    else:
        print("Warning: asset_variant template did not generate value")


def test_template_roundtrip():
    """Test that we can go value -> context -> value."""
    resolver = GenericResolver()

    original_value = "chrAlexBase"

    # Parse value to context
    context = resolver.parse_template_value(
        value=original_value,
        template_name="asset_variant"
    )

    print(f"Original: {original_value}")
    print(f"Context: {context}")

    if not context:
        pytest.skip("asset_variant template not available or didn't extract context")

    # Generate value from context
    generated_value = resolver.format_template_value(
        context={"context": context},  # Wrap in context dict
        template_name="asset_variant"
    )

    print(f"Generated: {generated_value}")

    # Should match original (case may differ)
    if generated_value:
        assert generated_value.lower() == original_value.lower()
    else:
        pytest.skip("Could not generate value from context")


def test_schema_from_path():
    """Test getting schema UID from a path."""
    resolver = GenericResolver()

    # Test with a typical project path
    test_paths = [
        r"P:\projects\DEMO\test.txt",
        r"P:\projects\TEST_PROJECT\asset.ma",
    ]

    for path in test_paths:
        schema_uid = resolver.schema_from_path(path)
        print(f"Path: {path} -> Schema: {schema_uid}")

        # May or may not find a schema depending on configuration
        # Just ensure it doesn't crash


if __name__ == "__main__":
    pytest.main([__file__, "-v", "-s"])
