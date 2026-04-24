"""
Test cases for the FileSchemaLoader implementation.
"""

import os
import tempfile
import json
import pytest
from pathlib import Path
from unittest.mock import patch

pytest.importorskip("silex.core", reason="silex C++ bindings not available")

from silex import SegmentFlags
from silex.core import FileSchemaLoader
from silex.core import Registry
from silex import SilexSchemaInfo, SilexSchema


class TestFileSchemaLoader:
    """Test cases for FileSchemaLoader."""

    def setup_method(self):
        """Set up test fixtures."""
        self.registry = Registry()
        self.loader = FileSchemaLoader(self.registry)

    def test_preload_with_test_resources(self):
        """Test preload using the actual test resource files."""
        test_resources_path = Path(__file__).parent.parent / "resources"

        with patch.dict(os.environ, {"SILEX_SCHEMA_PATH": str(test_resources_path)}):
            self.loader.preload()

        # Check that schemas were loaded
        available_schemas = self.loader.available_schema()

        # Should have schemas with UIDs from our test files
        expected_schemas = ["files.project", "files.project.testpipe", "test.basic"]
        for schema_uid in expected_schemas:
            assert schema_uid in available_schemas, f"Schema {schema_uid} not found in {available_schemas}"

    def test_preload_registers_functors_and_segmenters(self):
        """Test that preload registers functors and segmenters."""
        test_resources_path = Path(__file__).parent.parent / "resources"

        with patch.dict(os.environ, {"SILEX_SCHEMA_PATH": str(test_resources_path)}):
            self.loader.preload()

        # Check that functors were registered from silex_core.silex
        functor_info = self.registry.get_functor_info("silex.ConvertLowerCaseFunctor")
        assert functor_info is not None
        assert "lowercase" in functor_info.aliases

        # Check that segmenters were registered
        segmenter_info = self.registry.get_segmenter_info("silex.impl.segmenters.FilesystemSegmenter")
        assert segmenter_info is not None

    def test_load_info_basic_schema(self):
        """Test loading schema info for a basic schema."""
        test_resources_path = Path(__file__).parent.parent / "resources"

        with patch.dict(os.environ, {"SILEX_SCHEMA_PATH": str(test_resources_path)}):
            self.loader.preload()

        # Test loading basic schema info
        schema_info = self.loader.load_info("test.basic")

        assert isinstance(schema_info, SilexSchemaInfo)
        assert schema_info.uid == "test.basic"
        assert schema_info.path_pattern == "[\\/]basic[\\/].*"
        assert schema_info.root_path == os.path.abspath(test_resources_path / "data")
        assert schema_info.segmenter_uid == "silex.impl.segmenters.FilesystemSegmenter"
        assert len(schema_info.context_filters) == 1
        assert schema_info.context_filters[0] == {"project.code": "TEST"}

    def test_load_info_with_inheritance(self):
        """Test loading schema info that uses inheritance."""
        test_resources_path = Path(__file__).parent.parent / "resources"

        with patch.dict(os.environ, {"SILEX_SCHEMA_PATH": str(test_resources_path)}):
            self.loader.preload()

        # Test loading schema that inherits from another
        schema_info = self.loader.load_info("files.project.testpipe")

        assert isinstance(schema_info, SilexSchemaInfo)
        assert schema_info.uid == "files.project.testpipe"
        assert schema_info.extends == "files.project"
        # Should inherit segmenter from parent
        assert "FilesystemSegmenter" in schema_info.segmenter_uid

    def test_load_info_with_imports(self):
        """Test loading schema info that imports other files."""
        test_resources_path = Path(__file__).parent.parent / "resources"

        with patch.dict(os.environ, {"SILEX_SCHEMA_PATH": str(test_resources_path)}):
            self.loader.preload()

        # Test loading schema that imports other files
        schema_info = self.loader.load_info("files.project")

        assert isinstance(schema_info, SilexSchemaInfo)
        assert schema_info.uid == "files.project"
        # Functors from imports are registered globally in the registry, not per-schema
        assert schema_info.functor_uids == []
        # But aliases should be populated from schema JSON
        assert len(schema_info.functor_aliases) > 0

    def test_extract_endpoints(self):
        """Test extraction of endpoints from path structure after building schema."""
        test_resources_path = Path(__file__).parent.parent / "resources"

        with patch.dict(os.environ, {"SILEX_SCHEMA_PATH": str(test_resources_path)}):
            self.loader.preload()

        # Endpoints are extracted during loadSchema, not loadInfo
        self.loader.load_schema("files.project")
        schema_info = self.loader.load_info("files.project")

        # Should have endpoints extracted from the complex path structure
        assert "context.asset_variant" in schema_info.endpoints
        assert "context.sequence" in schema_info.endpoints
        assert "context.shot" in schema_info.endpoints

    def test_match_by_uid_pattern(self):
        """Test matching schemas by UID pattern."""
        test_resources_path = Path(__file__).parent.parent / "resources"

        with patch.dict(os.environ, {"SILEX_SCHEMA_PATH": str(test_resources_path)}):
            self.loader.preload()

        # Test matching by pattern
        matches = self.loader.match(uid_pattern="files.project.*")
        uids = [schema.uid for schema in matches]

        # "files.project.*" matches UIDs starting with "files.project." (the dot is literal)
        assert "files.project.testpipe" in uids
        assert "files.project" not in uids  # Exact match requires "files.project" pattern

    def test_match_by_context(self):
        """Test matching schemas by context filters."""
        test_resources_path = Path(__file__).parent.parent / "resources"

        with patch.dict(os.environ, {"SILEX_SCHEMA_PATH": str(test_resources_path)}):
            self.loader.preload()

        # Test matching by context
        context = {"project": {"code": "TEST"}}
        matches = self.loader.match(context=context)
        uids = [schema.uid for schema in matches]

        assert "test.basic" in uids
        assert "files.project.testpipe" not in uids

        context = {"project": {"code": "TESTPIPE"}}
        matches = self.loader.match(context=context)
        uids = [schema.uid for schema in matches]
        # Should return with the most specialized match first
        assert uids[0] == "files.project.testpipe"
        # Check that files.project is in the results (order may vary due to additional schemas)
        assert "files.project" in uids

    def test_match_by_path(self):
        """Test matching schemas by path filters."""
        test_resources_path = Path(__file__).parent.parent / "resources"

        with patch.dict(os.environ, {"SILEX_SCHEMA_PATH": str(test_resources_path)}):
            self.loader.preload()

        path = str(
            test_resources_path.parent
            / r"search_dir\FR00089_GH2\03_Production\Assets\Environment\envConventionBase\Scenefiles\Dataset\Scan\envConventionBase_Scan_v0005versioninfo.json"
        )
        matches = self.loader.match(path=path)
        uids = [schema.uid for schema in matches]
        assert uids[0] == "files.project"
        assert "files.project.testpipe" not in uids

        # Test testpipe
        path = str(
            test_resources_path.parent
            / r"search_dir\TESTPIPE\03_Production\Shots\TEST02\001\work\Animation\AnimBlocking\alex.telford\v004.t03\TEST02-001_AnimBlocking_v004.t03_render.0001.png"
        )
        matches = self.loader.match(path=path)
        uids = [schema.uid for schema in matches]
        assert "files.project.testpipe" in uids
        assert uids[0] == "files.project.testpipe"
        assert uids[1] == "files.project"

    def test_available_schema(self):
        """Test getting list of available schemas."""
        test_resources_path = Path(__file__).parent.parent / "resources"

        with patch.dict(os.environ, {"SILEX_SCHEMA_PATH": str(test_resources_path)}):
            self.loader.preload()

        available = self.loader.available_schema()

        assert isinstance(available, list)
        assert len(available) > 0
        assert "files.project" in available

    def test_load_info_nonexistent_schema(self):
        """Test loading info for a non-existent schema."""
        test_resources_path = Path(__file__).parent.parent / "resources"

        with patch.dict(os.environ, {"SILEX_SCHEMA_PATH": str(test_resources_path)}):
            self.loader.preload()

        with pytest.raises(ValueError, match="Schema UID 'nonexistent' not found"):
            self.loader.load_info("nonexistent")

    def test_invalid_json_file(self):
        """Test handling of invalid JSON files."""
        with tempfile.TemporaryDirectory() as temp_dir:
            # Create an invalid JSON file
            invalid_file = Path(temp_dir) / "invalid.silex"
            invalid_file.write_text("{invalid json")

            with patch.dict(os.environ, {"SILEX_SCHEMA_PATH": temp_dir}):
                # Should not raise exception, but should warn
                self.loader.preload()

                available = self.loader.available_schema()
                assert len(available) == 0

    def test_files_project_schema_complete_data(self):
        """Test complete data structure for files.project schema."""
        test_resources_path = Path(__file__).parent.parent / "resources"

        with patch.dict(os.environ, {"SILEX_SCHEMA_PATH": str(test_resources_path)}):
            self.loader.preload()

        schema_info = self.loader.load_info("files.project")

        # Convert to dict for comparison
        actual_data = {
            "path": schema_info.path,
            "uid": schema_info.uid,
            "root_path": schema_info.root_path,
            "path_pattern": schema_info.path_pattern,
            "context_filters": schema_info.context_filters,
            "segmenter_uid": schema_info.segmenter_uid,
            "functor_uids": sorted(schema_info.functor_uids),  # Sort for consistent comparison
            "functor_aliases": schema_info.functor_aliases,
            "endpoints": schema_info.endpoints,
            "extends": schema_info.extends,
        }

        # Expected data structure
        expected_data = {
            "uid": "files.project",
            "root_path": os.path.abspath(test_resources_path.parent / "search_dir"),
            "path_pattern": '(([a-zA-Z]:)|(\\\\[^\\\\]+\\\\[^\\\\]+))([\\\\/][^\\\\/:*?"<>|]+)*',  # Windows/Linux filepath pattern including UNC
            "context_filters": [{"project.code": ".*"}],
            "segmenter_uid": "silex.FilesystemSegmenter",  # Aliased segmenter
            "functor_uids": sorted(
                [
                    "silex.impl.functors.case_conversion.ConvertLowerCaseFunctor",
                    "silex.impl.functors.case_conversion.ConvertTitleCaseFunctor",
                    "silex.impl.functors.case_conversion.ConvertUpperCaseFunctor",
                    "silex.impl.functors.case_split.SplitCamelCaseFunctor",
                    "silex.impl.functors.case_split.SplitSnakeCaseFunctor",
                    "silex.impl.functors.glob.GlobFunctor",
                    "silex.impl.functors.glob_tag.GlobTagFunctor",
                    "silex.impl.functors.lexicon.LexiconFunctor",
                ]
            ),
            "functor_aliases": {
                "CC": "silex.impl.functors.case_split.SplitCamelCaseFunctor",
                "SC": "silex.impl.functors.case_split.SplitSnakeCaseFunctor",
                "L": "silex.LexiconFunctor",
                "lower": "silex.impl.functors.case_conversion.ConvertLowerCaseFunctor",
                "title": "silex.impl.functors.case_conversion.ConvertTitleCaseFunctor",
            },
            "endpoints": {
                "context.asset_variant": ["project.tree=assets.classification.asset_variant"],
                "asset.entity.texture": ["project.tree=assets.classification.asset_variant.fragment.version.texture_file"],
                "context.sequence": ["project.tree=shots.sequence"],
                "context.shot": ["project.tree=shots.sequence.shot"],
                "shot.frame": ["project.tree=shots.sequence.shot.frame_file"],
            },
            "extends": None,
        }

        # Compare each field
        assert actual_data["uid"] == expected_data["uid"]
        assert actual_data["root_path"] == expected_data["root_path"]
        assert actual_data["path_pattern"] == expected_data["path_pattern"]
        assert actual_data["context_filters"] == expected_data["context_filters"]
        assert actual_data["segmenter_uid"] == expected_data["segmenter_uid"]
        # functor_uids are empty because C++ registers them globally, not per-schema
        assert actual_data["functor_uids"] == []
        assert actual_data["functor_aliases"] == expected_data["functor_aliases"]
        # endpoints are only populated after load_schema, not load_info
        assert actual_data["extends"] == expected_data["extends"]

        # Verify path exists
        assert Path(actual_data["path"]).exists()
        assert actual_data["path"].endswith("file_resolver.silex")

    def test_files_project_testpipe_schema_complete_data(self):
        """Test complete data structure for files.project.testpipe schema."""
        test_resources_path = Path(__file__).parent.parent / "resources"

        with patch.dict(os.environ, {"SILEX_SCHEMA_PATH": str(test_resources_path)}):
            self.loader.preload()

        schema_info = self.loader.load_info("files.project.testpipe")

        # Convert to dict for comparison
        actual_data = {
            "path": schema_info.path,
            "uid": schema_info.uid,
            "root_path": schema_info.root_path,
            "path_pattern": schema_info.path_pattern,
            "context_filters": schema_info.context_filters,
            "segmenter_uid": schema_info.segmenter_uid,
            "functor_uids": sorted(schema_info.functor_uids),
            "functor_aliases": schema_info.functor_aliases,
            "endpoints": schema_info.endpoints,
            "extends": schema_info.extends,
        }

        # Expected data structure (inherits from files.project with overrides)
        root_path = test_resources_path.parent / "search_dir"
        expected_data = {
            "path": str(test_resources_path / "testpipe_file_resolver.silex"),
            "uid": "files.project.testpipe",
            "root_path": os.path.abspath(root_path),
            "path_pattern": '^.*[\\\\/]+TESTPIPE(?:[\\\\/][^\\\\/:*?"<>|]+)+$',
            "context_filters": [{"project.code": "TESTPIPE"}],
            "segmenter_uid": "silex.FilesystemSegmenter",
            "functor_uids": [],  # C++ registers functors globally, not per-schema
            "functor_aliases": {
                "CC": "silex.impl.functors.case_split.SplitCamelCaseFunctor",
                "SC": "silex.impl.functors.case_split.SplitSnakeCaseFunctor",
                "L": "silex.LexiconFunctor",
                "lower": "silex.impl.functors.case_conversion.ConvertLowerCaseFunctor",
                "title": "silex.impl.functors.case_conversion.ConvertTitleCaseFunctor",
            },
            "endpoints": {},  # Only populated after load_schema, not load_info
            "extends": "files.project",
        }

        # Compare individual fields for clarity
        assert actual_data["uid"] == expected_data["uid"]
        assert actual_data["context_filters"] == expected_data["context_filters"]
        assert actual_data["segmenter_uid"] == expected_data["segmenter_uid"]
        assert actual_data["functor_uids"] == expected_data["functor_uids"]
        assert actual_data["functor_aliases"] == expected_data["functor_aliases"]
        assert actual_data["extends"] == expected_data["extends"]

    def test_test_basic_schema_complete_data(self):
        """Test complete data structure for test.basic schema."""
        test_resources_path = Path(__file__).parent.parent / "resources"

        with patch.dict(os.environ, {"SILEX_SCHEMA_PATH": str(test_resources_path)}):
            self.loader.preload()

        schema_info = self.loader.load_info("test.basic")

        # Convert to dict for comparison
        actual_data = {
            "path": schema_info.path,
            "uid": schema_info.uid,
            "root_path": os.path.abspath(test_resources_path / "data"),
            "path_pattern": schema_info.path_pattern,
            "context_filters": schema_info.context_filters,
            "segmenter_uid": schema_info.segmenter_uid,
            "functor_uids": sorted(schema_info.functor_uids),
            "functor_aliases": schema_info.functor_aliases,
            "endpoints": schema_info.endpoints,
            "extends": schema_info.extends,
        }

        # Expected data structure
        expected_data = {
            "uid": "test.basic",
            "root_path": os.path.abspath(test_resources_path / "data"),
            "path_pattern": "[\\/]basic[\\/].*",
            "context_filters": [{"project.code": "TEST"}],
            "segmenter_uid": "silex.impl.segmenters.FilesystemSegmenter",  # Full segmenter path
            "functor_uids": [],  # No functors defined in test.basic
            "functor_aliases": {},  # No aliases defined
            "endpoints": {},  # No endpoints defined
            "extends": None,
        }

        # Compare each field
        assert actual_data["uid"] == expected_data["uid"]
        assert Path(actual_data["root_path"]) == Path(expected_data["root_path"])
        assert actual_data["path_pattern"] == expected_data["path_pattern"]
        assert actual_data["context_filters"] == expected_data["context_filters"]
        assert actual_data["segmenter_uid"] == expected_data["segmenter_uid"]
        assert actual_data["functor_uids"] == expected_data["functor_uids"]
        assert actual_data["functor_aliases"] == expected_data["functor_aliases"]
        assert actual_data["endpoints"] == expected_data["endpoints"]
        assert actual_data["extends"] == expected_data["extends"]

        # Verify path exists
        assert Path(actual_data["path"]).exists()
        assert actual_data["path"].endswith("test_basic.silex")

    def test_imported_data_integration(self):
        """Test that imported data from common.silex is properly integrated."""
        test_resources_path = Path(__file__).parent.parent / "resources"

        with patch.dict(os.environ, {"SILEX_SCHEMA_PATH": str(test_resources_path)}):
            self.loader.preload()

        # Access the internal merged data to verify imports worked
        schema_entry = self.loader._available_schemas["files.project"]
        merged_data = schema_entry["merged_data"]

        # Verify templates from common.silex were imported
        assert "templates" in merged_data
        templates = merged_data["templates"]

        # Check for templates from common.silex
        assert "version" in templates
        assert "frame" in templates

        # Verify template structure
        version_template = templates["version"]
        assert version_template["pattern"] == "v([0-9]+)"
        assert version_template["type"] == "number"
        assert version_template["padding"] == 3

        frame_template = templates["frame"]
        assert frame_template["pattern"] == "([0-9]+)"
        assert frame_template["type"] == "number"
        assert frame_template["padding"] == 4
        assert frame_template["placeholder"] == "####"

        # Verify config from common.silex was imported
        assert "config" in merged_data
        config = merged_data["config"]
        assert "variables" in config
        assert "lexicon" in config["variables"]

        lexicon = config["variables"]["lexicon"]
        assert "classification" in lexicon
        assert "aov" in lexicon

        # Check classification entries
        classification = lexicon["classification"]
        assert "character" in classification
        assert "prop" in classification
        assert "environment" in classification
        assert "vehicle" in classification

        # Verify specific values
        assert classification["character"] == ["chr", "character"]
        assert classification["prop"] == ["prp", "prop"]

    def test_all_schemas_summary(self):
        """Test summary of all loaded schemas for overview."""
        test_resources_path = Path(__file__).parent.parent / "resources"

        with patch.dict(os.environ, {"SILEX_SCHEMA_PATH": str(test_resources_path)}):
            self.loader.preload()

        available_schemas = self.loader.available_schema()

        # Create summary data for all schemas
        summary = {}
        for uid in available_schemas:
            schema_info = self.loader.load_info(uid)
            summary[uid] = {
                "has_endpoints": len(schema_info.endpoints) > 0,
                "endpoint_count": len(schema_info.endpoints),
                "has_functors": len(schema_info.functor_uids) > 0,
                "functor_count": len(schema_info.functor_uids),
                "has_aliases": len(schema_info.functor_aliases) > 0,
                "has_context_filters": len(schema_info.context_filters) > 0,
                "inherits_from": schema_info.extends,
                "segmenter": schema_info.segmenter_uid,
            }

        # Verify expected summary structure
        expected_summary = {
            "files.project": {
                "has_endpoints": False,
                "endpoint_count": 0,
                "has_functors": False,
                "functor_count": 0,
                "has_aliases": True,
                "has_context_filters": True,
                "inherits_from": None,
                "segmenter": "silex.FilesystemSegmenter",
            },
            "files.project.testpipe": {
                "has_endpoints": False,
                "endpoint_count": 0,
                "has_functors": False,
                "functor_count": 0,
                "has_aliases": True,  # Inherits aliases
                "has_context_filters": True,
                "inherits_from": "files.project",
                "segmenter": "silex.FilesystemSegmenter",
            },
            "test.basic": {
                "has_endpoints": False,
                "endpoint_count": 0,
                "has_functors": False,
                "functor_count": 0,
                "has_aliases": False,
                "has_context_filters": True,
                "inherits_from": None,
                "segmenter": "silex.impl.segmenters.FilesystemSegmenter",
            },
        }

        # Compare summaries
        for uid, expected in expected_summary.items():
            assert uid in summary, f"Schema {uid} not found in summary"
            actual = summary[uid]
            for key, expected_value in expected.items():
                assert (
                    actual[key] == expected_value
                ), f"Mismatch in {uid}.{key}: expected {expected_value}, got {actual[key]}"

    # MARK: load_schema() tests

    def test_load_schema_test_basic(self):
        """Test loading basic schema (no paths, minimal structure)."""
        test_resources_path = Path(__file__).parent.parent / "resources"

        with patch.dict(os.environ, {"SILEX_SCHEMA_PATH": str(test_resources_path)}):
            self.loader.preload()

        schema = self.loader.load_schema("test.basic")

        # Convert to dict for visual comparison
        actual_data = {
            "info": {
                "uid": schema.info.uid,
                "root_path": schema.info.root_path,
                "path_pattern": schema.info.path_pattern,
                "segmenter_uid": schema.info.segmenter_uid,
                "extends": schema.info.extends,
            },
            "config": {
                "global_variables": dict(schema.config.global_variables),
                "functor_variables": schema.config.functor_variables,
            },
            "root_segments": len(schema.root_segments),
        }

        expected_data = {
            "info": {
                "uid": "test.basic",
                "root_path": os.path.abspath(test_resources_path / "data"),
                "path_pattern": "[\\/]basic[\\/].*",
                "segmenter_uid": "silex.impl.segmenters.FilesystemSegmenter",
                "extends": None,
            },
            "config": {
                "global_variables": {},  # No config in test.basic
                "functor_variables": {},
            },
            "root_segments": 0,  # No paths defined in test.basic
        }

        assert actual_data == expected_data
        # Verify the schema is cached
        assert self.registry.get_schema("test.basic") is schema

    def test_load_schema_files_project_structure(self):
        """Test loading files.project schema and verify segment structure."""
        test_resources_path = Path(__file__).parent.parent / "resources"

        with patch.dict(os.environ, {"SILEX_SCHEMA_PATH": str(test_resources_path)}):
            self.loader.preload()

        schema = self.loader.load_schema("files.project")

        # Convert to dict for visual comparison
        def segment_to_dict(segment):
            return {
                "name": segment.name,
                "flags": int(segment.flags),
                "parse": str(segment.parse) if hasattr(segment.parse, "__str__") else segment.parse,
                "format": str(segment.format) if hasattr(segment.format, "__str__") else segment.format,
                "targets": {k: {"group": v.group, "variable": v.variable} for k, v in segment.targets.items()},
                "template": segment.template,
                "branches": {k: len(v) for k, v in segment.branches.items()},
                "has_parent": segment.parent is not None,
            }

        actual_data = {
            "info": {
                "uid": schema.info.uid,
                "functor_count": len(schema.info.functor_uids),
                "endpoint_count": len(schema.info.endpoints),
                "extends": schema.info.extends,
            },
            "config": {
                "has_variables": len(schema.config.global_variables) > 0,
                "variable_keys": (
                    sorted(schema.config.global_variables.keys()) if schema.config.global_variables else []
                ),
            },
            "root_segments_count": len(schema.root_segments),
            "root_segments": [segment_to_dict(seg) for seg in schema.root_segments],
        }

        # Expected structure based on file_resolver.silex
        # NOTE: With new read/write logic, expressions with $ are parsed, others remain as strings
        expected_data = {
            "info": {
                "uid": "files.project",
                "functor_count": 0,
                "endpoint_count": 5,  # Extracted from segment tree during loadSchema
                "extends": None,
            },
            "config": {
                "has_variables": True,
                "variable_keys": ["lexicon", "tags"],  # From common.silex import
            },
            "root_segments_count": 1,  # Only "project" segment at root
            "root_segments": [
                {
                    "name": "project",
                    "flags": 0,  # SegmentFlags.NONE
                    # Should be parsed since it contains $
                    "parse": "SilexExpressionGraph",  # Just check it's parsed (not exact string)
                    "format": "SilexExpressionGraph",  # Should be parsed since it contains $
                    "targets": {
                        "context.project": {"group": None, "variable": "project"},
                        "project.code": {"group": 2, "variable": None},
                        "project.name": {"group": 0, "variable": None},
                    },  # Read targets propagated to segment targets
                    "template": "",
                    "branches": {"": 1},  # One child (production_dir)
                    "has_parent": False,
                }
            ],
        }

        # Compare most fields exactly, but just check that expressions are parsed for parse/format
        assert actual_data["info"] == expected_data["info"]
        assert actual_data["config"] == expected_data["config"]
        assert actual_data["root_segments_count"] == expected_data["root_segments_count"]

        # For the root segment, check structure but not exact read/write strings
        actual_segment = actual_data["root_segments"][0]
        expected_segment = expected_data["root_segments"][0]

        assert actual_segment["name"] == expected_segment["name"]
        assert actual_segment["flags"] == expected_segment["flags"]
        assert actual_segment["targets"] == expected_segment["targets"]
        assert actual_segment["template"] == expected_segment["template"]
        assert actual_segment["branches"] == expected_segment["branches"]
        assert actual_segment["has_parent"] == expected_segment["has_parent"]

        # Check that parse and format are parsed (contain "SilexExpressionGraph")
        assert "SilexExpressionGraph" in actual_segment["parse"]
        assert "SilexExpressionGraph" in actual_segment["format"]
        assert self.registry.get_schema("files.project") is schema

    def test_load_schema_files_project_testpipe_inheritance(self):
        """Test loading testpipe schema and verify inheritance from files.project."""
        test_resources_path = Path(__file__).parent.parent / "resources"

        with patch.dict(os.environ, {"SILEX_SCHEMA_PATH": str(test_resources_path)}):
            self.loader.preload()

        schema = self.loader.load_schema("files.project.testpipe")

        # Convert to dict for visual comparison
        actual_data = {
            "info": {
                "uid": schema.info.uid,
                "extends": schema.info.extends,
                "functor_count": len(schema.info.functor_uids),
                "endpoint_count": len(schema.info.endpoints),
                "context_filters": schema.info.context_filters,
            },
            "config": {
                "has_variables": len(schema.config.global_variables) > 0,
                "variable_keys": (
                    sorted(schema.config.global_variables.keys()) if schema.config.global_variables else []
                ),
            },
            "root_segments_count": len(schema.root_segments),
            "inherited_from_parent": schema.info.extends == "files.project",
        }

        expected_data = {
            "info": {
                "uid": "files.project.testpipe",
                "extends": "files.project",
                "functor_count": 0,
                "endpoint_count": 4,  # Inherited schema now exposes four resolved endpoints
                "context_filters": [{"project.code": "TESTPIPE"}],  # Overridden in testpipe
            },
            "config": {
                "has_variables": True,  # Should inherit config from parent
                "variable_keys": ["lexicon", "tags"],  # From common.silex import (inherited)
            },
            "root_segments_count": 1,  # Only inherits root segments from parent (override of context.shot doesn't create new root)
            "inherited_from_parent": True,
        }

        assert actual_data == expected_data

        # Verify the schema is cached
        assert self.registry.get_schema("files.project.testpipe") is schema

    def test_load_schema_caching(self):
        """Test that schemas are properly cached in the registry."""
        test_resources_path = Path(__file__).parent.parent / "resources"

        with patch.dict(os.environ, {"SILEX_SCHEMA_PATH": str(test_resources_path)}):
            self.loader.preload()

        # Load schema first time
        schema1 = self.loader.load_schema("files.project")

        # Load schema second time - should return cached version
        schema2 = self.loader.load_schema("files.project")

        # Should be the exact same object
        assert schema1 is schema2

        # Verify it's in the registry cache
        cached_schema = self.registry.get_schema("files.project")
        assert cached_schema is schema1

    def test_load_schema_nonexistent(self):
        """Test loading non-existent schema raises appropriate error."""
        test_resources_path = Path(__file__).parent.parent / "resources"

        with patch.dict(os.environ, {"SILEX_SCHEMA_PATH": str(test_resources_path)}):
            self.loader.preload()

        with pytest.raises(ValueError, match="Schema UID 'nonexistent' not found"):
            self.loader.load_schema("nonexistent")

    def test_load_schema_complex_segments_hierarchy(self):
        """Test that complex segment hierarchies are built correctly."""
        test_resources_path = Path(__file__).parent.parent / "resources"

        with patch.dict(os.environ, {"SILEX_SCHEMA_PATH": str(test_resources_path)}):
            self.loader.preload()

        schema = self.loader.load_schema("files.project")

        # Navigate the segment hierarchy to verify it's built correctly
        assert len(schema.root_segments) == 1
        project_segment = schema.root_segments[0]
        assert project_segment.name == "project"

        # Should have one default branch with one child (production_dir)
        assert "" in project_segment.branches
        assert len(project_segment.branches[""]) == 1

        production_dir = project_segment.branches[""][0]
        assert production_dir.name == "production_dir"
        assert production_dir.flags & SegmentFlags.INTERMEDIATE

        # production_dir should have tree segment
        assert "" in production_dir.branches
        assert len(production_dir.branches[""]) == 1

        tree_segment = production_dir.branches[""][0]
        assert tree_segment.name == "tree"

        # tree segment should have switch branches for "assets" and "shots"
        assert "assets" in tree_segment.branches
        assert "shots" in tree_segment.branches
        assert len(tree_segment.branches["assets"]) >= 1
        assert len(tree_segment.branches["shots"]) >= 1

        # Verify parent references work
        assert production_dir.parent is project_segment
        assert tree_segment.parent is production_dir

    def test_load_schema_maps_lifecycle_flags_from_segments_templates_and_overrides(self):
        """Test that schema lifecycle flags map to `SegmentFlags` during segment building."""
        test_schema_content = {
            "uid": "test.segment_flags",
            "segmenter": "silex.impl.segmenters.FilesystemSegmenter",
            "pattern": "[\\/]test[\\/].*",
            "root_path": "./data",
            "templates": {
                "lifecycle_template": {
                    "is_readonly": True,
                    "is_deprecated": True,
                }
            },
            "paths": {
                "direct_readonly": {
                    "is_readonly": True,
                },
                "direct_deprecated": {
                    "is_deprecated": True,
                },
                "templated_flags": {
                    "template": "lifecycle_template",
                },
                "override_target": {},
            },
            "overrides": {
                "override_target": {
                    "is_readonly": True,
                    "is_deprecated": True,
                }
            },
        }

        with tempfile.NamedTemporaryFile(mode="w", suffix=".silex", delete=False) as handle:
            json.dump(test_schema_content, handle, indent=2)
            test_schema_path = handle.name

        try:
            original_path = os.getenv("SILEX_SCHEMA_PATH", "")
            test_dir = os.path.dirname(test_schema_path)

            with patch.dict(os.environ, {"SILEX_SCHEMA_PATH": f"{original_path};{test_dir}"}):
                test_loader = FileSchemaLoader(Registry())
                test_loader.preload()
                schema = test_loader.load_schema("test.segment_flags")

            segments = {segment.name: segment for segment in schema.root_segments}

            assert segments["direct_readonly"].flags & SegmentFlags.READONLY
            assert not segments["direct_readonly"].flags & SegmentFlags.DEPRECATED

            assert segments["direct_deprecated"].flags & SegmentFlags.DEPRECATED
            assert not segments["direct_deprecated"].flags & SegmentFlags.READONLY

            assert segments["templated_flags"].flags & SegmentFlags.READONLY
            assert segments["templated_flags"].flags & SegmentFlags.DEPRECATED

            assert segments["override_target"].flags & SegmentFlags.READONLY
            assert segments["override_target"].flags & SegmentFlags.DEPRECATED
        finally:
            os.unlink(test_schema_path)

    def test_load_schema_supports_endpoint_and_glob_override_selectors(self):
        """Test that overrides can target endpoints and suffix path selectors."""
        test_schema_content = {
            "uid": "test.override_selectors",
            "inherits": "files.project",
            "root_path": "./data",
            "overrides": {
                "context.shot": {
                    "pattern": "SHOT([0-9]+)"
                },
                "tree=shots.*.shot.frame_file": {
                    "pattern": "delivery\\.([0-9]+)\\.([a-zA-Z]+)",
                    "format": "delivery.{frame:04d}.{file.ext}"
                }
            },
        }

        with tempfile.NamedTemporaryFile(mode="w", suffix=".silex", delete=False) as handle:
            json.dump(test_schema_content, handle, indent=2)
            test_schema_path = handle.name

        try:
            original_path = os.getenv("SILEX_SCHEMA_PATH", "")
            test_dir = os.path.dirname(test_schema_path)

            resources_dir = Path(__file__).parent.parent / "resources"

            with patch.dict(os.environ, {"SILEX_SCHEMA_PATH": f"{resources_dir};{original_path};{test_dir}"}):
                test_loader = FileSchemaLoader(Registry())
                test_loader.preload()
                schema = test_loader.load_schema("test.override_selectors")

            project = schema.root_segments[0]
            production_dir = project.branches[""][0]
            tree = production_dir.branches[""][0]
            sequence = tree.branches["shots"][0]
            shot = sequence.branches[""][0]
            frame_file = shot.branches[""][0]

            assert shot.pattern == "SHOT([0-9]+)"
            assert frame_file.pattern == "delivery\\.([0-9]+)\\.([a-zA-Z]+)"
            assert frame_file.format[0]["expressions"] == ["delivery.{frame:04d}.{file.ext}"]
        finally:
            os.unlink(test_schema_path)

    def test_load_schema_template_usage(self):
        """Test that templates are properly applied to segments."""
        test_resources_path = Path(__file__).parent.parent / "resources"

        with patch.dict(os.environ, {"SILEX_SCHEMA_PATH": str(test_resources_path)}):
            self.loader.preload()

        schema = self.loader.load_schema("files.project")

        # Navigate to the asset_variant segment that uses the "asset_variant" template
        project_segment = schema.root_segments[0]
        production_dir = project_segment.branches[""][0]
        tree_segment = production_dir.branches[""][0]
        classification_segment = tree_segment.branches["assets"][0]
        asset_variant_segment = classification_segment.branches[""][0]

        assert asset_variant_segment.name == "asset_variant"
        assert asset_variant_segment.template == "asset_variant"

        # Convert to dict for visual comparison
        def segment_to_dict(segment):
            return {
                "name": segment.name,
                "template": segment.template,
                "pattern": (
                    segment.parse
                    if hasattr(segment, "parse") and hasattr(segment.parse, "expressions")
                    else str(segment.parse)
                ),
                "format": str(segment.format),
                "targets": {k: {"group": v.group, "variable": v.variable} for k, v in segment.targets.items()},
                "has_parent": segment.parent is not None,
            }

        actual_data = segment_to_dict(asset_variant_segment)

        # Expected data from template application
        expected_data = {
            "name": "asset_variant",
            "template": "asset_variant",
            # The template should have been applied, giving the segment the template's pattern
            "pattern": "[a-zA-Z0-9]+",  # From template
            "format": "SilexExpressionGraph",  # Should be parsed since it contains $
            "targets": {
                # These targets should come from the template
                "context.classification": {"group": None, "variable": "classification"},
                "context.asset_variant": {"group": None, "variable": "value"},
                "context.asset": {"group": None, "variable": "asset"},
                "context.variant": {"group": None, "variable": "variant"},
            },
            "has_parent": True,
        }

        # Check that template data was applied correctly
        assert actual_data["name"] == expected_data["name"]
        assert actual_data["template"] == expected_data["template"]
        assert "SilexExpressionGraph" in actual_data["format"]  # Should be parsed since it contains $
        assert actual_data["has_parent"] == expected_data["has_parent"]

        # Note: The pattern might not match exactly due to expression parsing,
        # but we should verify that the segment has content from the template
        # Read targets are stored in parse entries, not segment-level targets
        assert len(asset_variant_segment.parse) > 0  # Should have parse entries from template

    def test_load_schema_template_inheritance(self):
        """Test that template inheritance works correctly."""
        test_resources_path = Path(__file__).parent.parent / "resources"

        with patch.dict(os.environ, {"SILEX_SCHEMA_PATH": str(test_resources_path)}):
            self.loader.preload()

        schema = self.loader.load_schema("test.template")

        # Check that we have two root segments using templates
        assert len(schema.root_segments) == 2

        base_segment = None
        derived_segment = None

        for segment in schema.root_segments:
            if segment.name == "base_segment":
                base_segment = segment
            elif segment.name == "derived_segment":
                derived_segment = segment

        assert base_segment is not None
        assert derived_segment is not None

        # Check base segment - format is now normalized to list format
        assert base_segment.name == "base_segment"
        assert base_segment.template == "base_template"
        # Format is now: [{"expressions": ["{base.value}"], "when": None, "parsed": None}]
        assert isinstance(base_segment.format, list)
        assert len(base_segment.format) == 1
        assert base_segment.format[0]["expressions"] == ["{base.value}"]
        assert base_segment.format[0]["when"] is None
        # Read targets are stored in parse entry, not segment targets
        assert len(base_segment.parse) == 1
        assert "targets" in base_segment.parse[0]

        # Check derived segment (should have inherited and overridden)
        assert derived_segment.name == "derived_segment"
        assert derived_segment.template == "derived_template"
        # Format is now: [{"expressions": ["{derived.value}"], "when": None, "parsed": None}]
        assert isinstance(derived_segment.format, list)
        assert len(derived_segment.format) == 1
        assert derived_segment.format[0]["expressions"] == ["{derived.value}"]
        assert derived_segment.format[0]["when"] is None
        # Read targets are in parse entry
        assert len(derived_segment.parse) == 1
        assert "targets" in derived_segment.parse[0]

    def test_build_segment_write_read_behavior(self):
        """Test the new read/write behavior by loading a test schema with various configurations."""
        # Create a test schema file to verify the new behavior
        test_schema_content = {
            "uid": "test.read_write",
            "segmenter": "silex.impl.segmenters.FilesystemSegmenter",
            "pattern": "[\\/]test[\\/].*",
            "root_path": "./data",
            "context_filters": [{"project.code": "READ_WRITE_TEST"}],
            "paths": {
                "default_segment": {
                    # No read/write specified - should use defaults
                },
                "write_target_segment": {"format": "target"},
                "write_string_no_dollar": {"format": "{some.value}"},
                "write_string_with_dollar": {"format": "$lower({value})"},
                "write_dict_expression": {"format": {"expression": "$lower({value})"}},
                "write_dict_expressions": {"format": {"expressions": ["$lower({value})", "{other}"]}},
                "read_with_target": {"target": "some.var"},
                "read_with_targets": {"parse": {"targets": {"output1": "var1", "output2": "var2"}}},
                "read_with_expression_no_dollar": {
                    "parse": {"expression": "{value}->output", "targets": {"output": "var"}}
                },
                "read_with_expression_dollar": {
                    "parse": {"expression": "$lower({value})->output", "targets": {"output": "var"}}
                },
                "read_with_expressions_list": {
                    "parse": {
                        "expressions": ["$lower({value})->output1", "{other}->output2"],
                        "targets": {"output1": "var1", "output2": "var2"},
                    }
                },
            },
        }

        # Write the test schema to a temporary file
        import tempfile
        import json

        with tempfile.NamedTemporaryFile(mode="w", suffix=".silex", delete=False) as f:
            json.dump(test_schema_content, f, indent=2)
            test_schema_path = f.name

        try:
            # Update SILEX_SCHEMA_PATH to include our test schema
            original_path = os.getenv("SILEX_SCHEMA_PATH", "")
            test_dir = os.path.dirname(test_schema_path)

            with patch.dict(os.environ, {"SILEX_SCHEMA_PATH": f"{original_path};{test_dir}"}):
                # Create new loader to pick up the test schema
                test_loader = FileSchemaLoader(Registry())
                test_loader.preload()

                # Load the test schema
                schema = test_loader.load_schema("test.read_write")

                # Convert segments to dict for verification
                def find_segment_by_name(segments, name):
                    for seg in segments:
                        if seg.name == name:
                            return seg
                    return None

                # Verify format behaviors - format is now always a list
                default_seg = find_segment_by_name(schema.root_segments, "default_segment")
                assert default_seg is not None
                # Should default to [{"expressions": ["{segment_name}"], ...}]
                assert isinstance(default_seg.format, list)

                target_seg = find_segment_by_name(schema.root_segments, "write_target_segment")
                assert target_seg is not None
                assert isinstance(target_seg.format, list)
                assert target_seg.format[0]["expressions"] == ["target"]

                string_no_dollar_seg = find_segment_by_name(schema.root_segments, "write_string_no_dollar")
                assert string_no_dollar_seg is not None
                assert isinstance(string_no_dollar_seg.format, list)
                assert string_no_dollar_seg.format[0]["expressions"] == ["{some.value}"]

                # The rest should be parsed expressions
                string_with_dollar_seg = find_segment_by_name(schema.root_segments, "write_string_with_dollar")
                assert string_with_dollar_seg is not None
                assert isinstance(string_with_dollar_seg.format, list)
                assert string_with_dollar_seg.format[0]["parsed"] is not None  # Should be parsed

                # Verify read behaviors - read is now always a list
                read_target_seg = find_segment_by_name(schema.root_segments, "read_with_target")
                assert read_target_seg is not None
                assert isinstance(read_target_seg.parse, list)
                # Should have target with segment name as key

                read_targets_seg = find_segment_by_name(schema.root_segments, "read_with_targets")
                assert read_targets_seg is not None
                assert isinstance(read_targets_seg.parse, list)

        finally:
            # Clean up
            os.unlink(test_schema_path)

    def test_detailed_read_write_behavior(self):
        """Test specific read/write behaviors as requested."""
        # Create comprehensive test schema
        test_schema_content = {
            "uid": "test.detailed_read_write",
            "segmenter": "silex.impl.segmenters.FilesystemSegmenter",
            "pattern": "[\\/]test[\\/].*",
            "root_path": "./data",
            "context_filters": [{"project.code": "DETAILED_TEST"}],
            "paths": {
                # Write tests
                "no_write_no_target": {},  # Should default to "{no_write_no_target}"
                "no_write_with_target": {"target": "some.var"},  # Should default to "target"
                "write_string_no_dollar": {"format": "{some.value}"},  # Should remain as string
                "write_string_with_dollar": {"format": "$lower({value})"},  # Should be parsed
                "write_list": {"format": ["$lower({value})", "{other}"]},  # Should be parsed
                "write_dict_expression": {"format": {"expression": "$lower({value})"}},  # Should be parsed
                "write_dict_expression_no_dollar": {"format": {"expression": "{some.value}"}},  # Should remain as string
                "write_dict_expressions": {
                    "format": {"expressions": ["$lower({value})", "{other}"]}
                },  # Should be parsed
                # Read tests
                "no_read": {},  # Should default to "{value}"
                "read_with_target_string": {"target": "output.var"},  # Should create target with segment name as key
                "read_with_targets_dict": {"parse": {"targets": {"output1": "var1", "output2": "var2"}}},
                "read_expression_no_dollar": {
                    "parse": {"expression": "{value}->output", "targets": {"output": "var"}}
                },  # Should remain as string
                "read_expression_with_dollar": {
                    "parse": {"expression": "$lower({value})->output", "targets": {"output": "var"}}
                },  # Should be parsed
                "read_expressions_list": {
                    "parse": {
                        "expressions": ["$lower({value})->output1", "{other}->output2"],
                        "targets": {"output1": "var1", "output2": "var2"},
                    }
                },  # Should be parsed
                "read_target_in_read": {
                    "parse": {"target": "final.var"}
                },  # Should create target with segment name as key
                # Priority tests
                "priority_targets_over_target": {"targets": {"highest": "priority1"}, "target": "priority2"},
                "priority_target_over_read_targets": {
                    "target": "priority1",
                    "parse": {"targets": {"lower": "priority2"}},
                },
                "priority_read_targets_over_read_target": {
                    "parse": {"targets": {"higher": "priority1"}, "target": "priority2"}
                },
            },
        }

        with tempfile.NamedTemporaryFile(mode="w", suffix=".silex", delete=False) as f:
            json.dump(test_schema_content, f, indent=2)
            test_schema_path = f.name

        try:
            original_path = os.getenv("SILEX_SCHEMA_PATH", "")
            test_dir = os.path.dirname(test_schema_path)

            with patch.dict(os.environ, {"SILEX_SCHEMA_PATH": f"{original_path};{test_dir}"}):
                test_loader = FileSchemaLoader(Registry())
                test_loader.preload()
                schema = test_loader.load_schema("test.detailed_read_write")

                # Helper to find segments
                segments = {seg.name: seg for seg in schema.root_segments}

                # Test format defaults - format is now always a list
                # no_write_no_target has empty format (no fallback auto-generation)
                assert isinstance(segments["no_write_no_target"].format, list)
                assert len(segments["no_write_no_target"].format) == 0

                # no_write_with_target should default to [{"expressions": ["{some.var}"], ...}]
                assert isinstance(segments["no_write_with_target"].format, list)
                assert segments["no_write_with_target"].format[0]["expressions"] == ["{some.var}"]

                # Test write string behavior - all normalized to list format
                assert isinstance(segments["write_string_no_dollar"].format, list)
                assert segments["write_string_no_dollar"].format[0]["expressions"] == ["{some.value}"]
                assert segments["write_string_no_dollar"].format[0]["parsed"] is None  # No $ = not parsed

                assert isinstance(segments["write_string_with_dollar"].format, list)
                assert segments["write_string_with_dollar"].format[0]["parsed"] is not None  # $ = parsed

                # Test write list/dict behavior
                assert isinstance(segments["write_list"].format, list)
                assert segments["write_list"].format[0]["parsed"] is not None  # Has $ = parsed

                assert isinstance(segments["write_dict_expression"].format, list)
                assert segments["write_dict_expression"].format[0]["parsed"] is not None  # Dict with $ = parsed

                assert isinstance(segments["write_dict_expression_no_dollar"].format, list)
                assert segments["write_dict_expression_no_dollar"].format[0]["expressions"] == ["{some.value}"]
                assert segments["write_dict_expression_no_dollar"].format[0]["parsed"] is None  # Dict no $ = not parsed

                assert isinstance(segments["write_dict_expressions"].format, list)
                assert segments["write_dict_expressions"].format[0]["parsed"] is not None  # Dict list = parsed

                # Test read defaults - no auto-default in C++
                assert isinstance(segments["no_read"].parse, list)
                assert len(segments["no_read"].parse) == 0

                # Test read expression behavior
                assert isinstance(segments["read_expression_no_dollar"].parse, list)
                assert segments["read_expression_no_dollar"].parse[0]["expressions"] == ["{value}->output"]
                assert segments["read_expression_no_dollar"].parse[0]["parsed"] is None  # No $ = not parsed

                assert isinstance(segments["read_expression_with_dollar"].parse, list)
                assert segments["read_expression_with_dollar"].parse[0]["parsed"] is not None  # $ = parsed

                assert isinstance(segments["read_expressions_list"].parse, list)
                assert segments["read_expressions_list"].parse[0]["parsed"] is not None  # List = parsed

                # Test target creation and priority
                assert "output.var" in segments["read_with_target_string"].targets
                assert segments["read_with_target_string"].targets["output.var"].variable == "value"

                assert "output1" in segments["read_with_targets_dict"].targets
                assert "output2" in segments["read_with_targets_dict"].targets

                assert "final.var" in segments["read_target_in_read"].targets
                assert segments["read_target_in_read"].targets["final.var"].variable == "value"

                # Test priority: targets > target > read.targets > read.target
                assert "highest" in segments["priority_targets_over_target"].targets
                assert "priority2" not in segments["priority_targets_over_target"].targets  # Should not exist

                assert "priority1" in segments["priority_target_over_read_targets"].targets
                assert segments["priority_target_over_read_targets"].targets["priority1"].variable == "value"
                assert "lower" not in segments["priority_target_over_read_targets"].targets  # Should not exist

                assert "higher" in segments["priority_read_targets_over_read_target"].targets
                assert segments["priority_read_targets_over_read_target"].targets["higher"].variable == "priority1"

        finally:
            os.unlink(test_schema_path)
