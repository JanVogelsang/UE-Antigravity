"""Isolation for pure unit tests.

Tests/conftest.py installs a session-scoped autouse fixture that launches the Unreal Editor and
provisions test Blueprints. Tests in this directory exercise plain Python helpers and need none
of that, so the editor-dependent fixtures are overridden with no-ops here. Overriding by name in
a nested conftest keeps the parent suite untouched.
"""
import pytest


@pytest.fixture(scope="session", autouse=True)
def setup_test_blueprint():
    yield None


@pytest.fixture(scope="session")
def unreal_process():
    yield None


@pytest.fixture(scope="session")
def mock_agent_client():
    pytest.skip("unit tests must not talk to the editor")
