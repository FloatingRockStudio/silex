import os
import pytest
from pathlib import Path

# Set SILEX_SCHEMA_PATH before any silex imports
test_resources = Path(__file__).parent / "resources"
os.environ["SILEX_SCHEMA_PATH"] = str(test_resources)


@pytest.fixture
def resources():
    """Fixture that returns the Path to the test resources directory."""
    return Path(__file__).parent / "resources"
