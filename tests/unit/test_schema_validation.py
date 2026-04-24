"""
Tests for Silex JSON Schema validation.

This module tests the JSON schema validation for .silex files to ensure they conform
to the expected structure and syntax rules.
"""

import json
import os
import re
import tempfile
from pathlib import Path
import pytest

pytest.importorskip("silex.core", reason="silex C++ bindings not available")

from jsonschema import validate, ValidationError, Draft202012Validator
from silex.core import FileSchemaLoader
from silex.core import Registry


def _strip_json_comments(text: str) -> str:
    """Strip single-line // comments and trailing commas from JSON5 text."""
    text = re.sub(r'(?<!:)//.*', '', text)
    text = re.sub(r',\s*([}\]])', r'\1', text)
    return text


class TestSilexSchemaValidation:
    """Test Silex schema file validation using JSON Schema."""

    @pytest.fixture
    def schema_validator(self):
        """Load the Silex JSON schema and create a validator."""
        schema_path = Path(__file__).parent.parent.parent / "schema" / "silex_schema.json"
        with open(schema_path, "r", encoding="utf-8") as f:
            schema = json.load(f)
        return Draft202012Validator(schema)

    @pytest.fixture
    def resources_path(self):
        """Get the test resources directory."""
        return Path(__file__).parent.parent / "resources"

    def test_schema_validator_loads(self, schema_validator):
        """Test that the JSON schema loads correctly."""
        assert schema_validator is not None
        assert schema_validator.schema is not None

    def test_validate_file_resolver_schema(self, schema_validator, resources_path):
        """Test validation of the file_resolver.silex schema."""
        silex_file = resources_path / "file_resolver.silex"

        with open(silex_file, "r", encoding="utf-8") as f:
            content = f.read()

        data = json.loads(_strip_json_comments(content))

        # Validate against schema
        errors = list(schema_validator.iter_errors(data))
        if errors:
            for error in errors[:5]:  # Show first 5 errors
                print(f"Validation error at {'.'.join(str(p) for p in error.absolute_path)}: {error.message}")

        assert len(errors) == 0, f"Schema validation failed with {len(errors)} errors"

    def test_validate_testpipe_file_resolver_schema(self, schema_validator, resources_path):
        """Test validation of the testpipe_file_resolver.silex schema."""
        silex_file = resources_path / "testpipe_file_resolver.silex"

        with open(silex_file, "r", encoding="utf-8") as f:
            content = f.read()

        data = json.loads(_strip_json_comments(content))

        # Validate against schema
        errors = list(schema_validator.iter_errors(data))
        if errors:
            for error in errors[:5]:  # Show first 5 errors
                print(f"Validation error at {'.'.join(str(p) for p in error.absolute_path)}: {error.message}")

        assert len(errors) == 0, f"Schema validation failed with {len(errors)} errors"

    def test_validate_common_schema(self, schema_validator, resources_path):
        """Test validation of the common.silex schema."""
        silex_file = resources_path / "common.silex"

        with open(silex_file, "r", encoding="utf-8") as f:
            content = f.read()

        data = json.loads(_strip_json_comments(content))

        # Validate against schema
        errors = list(schema_validator.iter_errors(data))
        if errors:
            for error in errors[:5]:  # Show first 5 errors
                print(f"Validation error at {'.'.join(str(p) for p in error.absolute_path)}: {error.message}")

        assert len(errors) == 0, f"Schema validation failed with {len(errors)} errors"

    def test_validate_silex_core_schema(self, schema_validator, resources_path):
        """Test validation of the silex_core.silex schema."""
        silex_file = resources_path / "silex_core.silex"

        with open(silex_file, "r", encoding="utf-8") as f:
            content = f.read()

        data = json.loads(_strip_json_comments(content))

        # Validate against schema
        errors = list(schema_validator.iter_errors(data))
        if errors:
            for error in errors[:5]:  # Show first 5 errors
                print(f"Validation error at {'.'.join(str(p) for p in error.absolute_path)}: {error.message}")

        assert len(errors) == 0, f"Schema validation failed with {len(errors)} errors"

    def test_validate_test_basic_schema(self, schema_validator, resources_path):
        """Test validation of the test_basic.silex schema."""
        silex_file = resources_path / "test_basic.silex"

        with open(silex_file, "r", encoding="utf-8") as f:
            content = f.read()

        data = json.loads(_strip_json_comments(content))

        # Validate against schema
        errors = list(schema_validator.iter_errors(data))
        if errors:
            for error in errors[:5]:  # Show first 5 errors
                print(f"Validation error at {'.'.join(str(p) for p in error.absolute_path)}: {error.message}")

        assert len(errors) == 0, f"Schema validation failed with {len(errors)} errors"

    def test_invalid_schema_detection(self, schema_validator):
        """Test that invalid schemas are properly detected."""
        # Test missing required fields
        invalid_data = {"invalid_field": "should not be allowed"}

        errors = list(schema_validator.iter_errors(invalid_data))
        assert len(errors) > 0, "Invalid schema should produce validation errors"

    def test_valid_minimal_schema(self, schema_validator):
        """Test that a minimal valid schema passes validation."""
        minimal_schema = {"uid": "test.minimal"}

        errors = list(schema_validator.iter_errors(minimal_schema))
        if errors:
            for error in errors:
                print(f"Validation error: {error.message}")

        assert len(errors) == 0, "Minimal valid schema should pass validation"

    def test_override_cascade_validation(self, schema_validator):
        """Test that override cascade values are validated correctly."""
        schema_with_overrides = {
            "uid": "test.overrides",
            "inherits": "base.schema",
            "overrides": {
                "project": {"cascade": "update", "pattern": "test.*"},
                "context.shot": {"cascade": "replace", "format": "{context.shot}"},
            },
        }

        errors = list(schema_validator.iter_errors(schema_with_overrides))
        if errors:
            for error in errors:
                print(f"Validation error: {error.message}")

        assert len(errors) == 0, "Schema with valid overrides should pass validation"

    def test_invalid_cascade_value(self, schema_validator):
        """Test that invalid cascade values are rejected."""
        schema_with_invalid_cascade = {
            "uid": "test.invalid",
            "overrides": {"project": {"cascade": "invalid_cascade_type", "pattern": "test.*"}},
        }

        errors = list(schema_validator.iter_errors(schema_with_invalid_cascade))
        assert len(errors) > 0, "Schema with invalid cascade value should be rejected"

        # Check that the error is about the cascade value
        cascade_errors = [e for e in errors if "cascade" in str(e.absolute_path)]
        assert len(cascade_errors) > 0, "Should have specific error about cascade value"

    def test_functor_definition_validation(self, schema_validator):
        """Test functor definition validation."""
        schema_with_functors = {
            "uid": "test.functors",
            "functors": [
                "silex.impl.functors.case_conversion.ConvertLowerCaseFunctor",
                {
                    "module": "silex.impl.functors.glob",
                    "name": "GlobFunctor",
                    "language": "python",
                    "aliases": ["glob"],
                },
            ],
        }

        errors = list(schema_validator.iter_errors(schema_with_functors))
        if errors:
            for error in errors:
                print(f"Validation error: {error.message}")

        assert len(errors) == 0, "Schema with valid functors should pass validation"

    def test_context_path_pattern_validation(self, schema_validator):
        """Test that context path patterns are validated correctly."""
        schema_with_paths = {
            "uid": "test.paths",
            "paths": {
                "project": {"pattern": "[a-zA-Z]+", "target": "context.project", "endpoint": "context.project_endpoint"}
            },
        }

        errors = list(schema_validator.iter_errors(schema_with_paths))
        if errors:
            for error in errors:
                print(f"Validation error: {error.message}")

        assert len(errors) == 0, "Schema with valid paths should pass validation"

    def test_lifecycle_flags_validate_in_segments_templates_and_overrides(self, schema_validator):
        """Test that lifecycle flags are accepted by schema validation."""
        schema_with_flags = {
            "uid": "test.lifecycle_flags",
            "templates": {
                "readonly_template": {
                    "is_readonly": True,
                    "is_deprecated": True,
                }
            },
            "paths": {
                "legacy_segment": {
                    "template": "readonly_template",
                    "is_deprecated": True,
                }
            },
            "overrides": {
                "legacy_segment": {
                    "is_readonly": True,
                }
            },
        }

        errors = list(schema_validator.iter_errors(schema_with_flags))

        assert len(errors) == 0, "Schema with lifecycle flags should pass validation"

    def test_integration_with_schema_loader(self, schema_validator):
        """Test that schemas loaded by FileSchemaLoader validate against our JSON schema."""
        # Create a temporary valid schema file
        valid_schema = {
            "uid": "test.integration",
            "functors": ["silex.impl.functors.glob.GlobFunctor"],
            "paths": {
                "project": {
                    "pattern": "[a-zA-Z]+",
                    "format": "{context.project}",
                    "parse": {"targets": {"context.project": "value"}},
                }
            },
        }

        # Create a temporary directory and file
        with tempfile.TemporaryDirectory() as temp_dir:
            temp_file = Path(temp_dir) / "test_integration.silex"
            with open(temp_file, "w", encoding="utf-8") as f:
                f.write(json.dumps(valid_schema, indent=2))

            # Load with schema loader by setting the schema path
            registry = Registry("test")
            loader = FileSchemaLoader(registry)

            # Set the schema path and preload
            with tempfile.NamedTemporaryFile(delete=False) as env_file:
                env_file.close()
                try:
                    import os

                    old_path = os.environ.get("SILEX_SCHEMA_PATH", "")
                    os.environ["SILEX_SCHEMA_PATH"] = str(temp_dir)

                    loader.preload()

                    # Load the schema info by UID
                    schema_info = loader.load_info("test.integration")
                    assert schema_info is not None
                    assert schema_info.uid == "test.integration"

                    # Validate the loaded data against our JSON schema
                    with open(temp_file, "r", encoding="utf-8") as f:
                        loaded_data = json.load(f)

                    errors = list(schema_validator.iter_errors(loaded_data))
                    if errors:
                        for error in errors:
                            print(f"Validation error: {error.message}")

                    assert len(errors) == 0, "Schema loaded by FileSchemaLoader should validate against JSON schema"

                finally:
                    # Restore environment
                    if old_path:
                        os.environ["SILEX_SCHEMA_PATH"] = old_path
                    elif "SILEX_SCHEMA_PATH" in os.environ:
                        del os.environ["SILEX_SCHEMA_PATH"]
                    os.unlink(env_file.name)
        # Cleanup
        registry.clear()  # Not actually needed
